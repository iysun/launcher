#include "terminalcore.h"

#include <QVector>
#include <algorithm>
#include <cstring>
#include <deque>

extern "C" {
#include "vterm.h"
}

// ── scrollback 历史行存储（唯一含 libvterm 类型的成员，故定义在 .cpp）─────
struct TerminalCore::Scrollback {
    static constexpr int kMaxLines = 5000; // 超限从最旧端驱逐

    struct Line {
        QVector<VTermScreenCell> cells;
    };
    std::deque<Line> lines;   // front = 最旧，back = 最新（紧邻屏幕首行）
    qint64           evicted = 0; // 已驱逐行数（全局行号基准）
};

// ── libvterm C 回调蹦床（user == TerminalCore*）──────────────────────
namespace {

void outputCb(const char *s, size_t len, void *user) {
    static_cast<TerminalCore *>(user)->sinkOutput(s, static_cast<int>(len));
}

int damageCb(VTermRect rect, void *user) {
    static_cast<TerminalCore *>(user)->sinkDamage(
        rect.start_row, rect.start_col, rect.end_row - rect.start_row,
        rect.end_col - rect.start_col);
    return 1;
}

int moveCursorCb(VTermPos pos, VTermPos /*oldpos*/, int visible, void *user) {
    static_cast<TerminalCore *>(user)->sinkCursor(pos.row, pos.col, visible != 0);
    return 1;
}

int setTermPropCb(VTermProp prop, VTermValue *val, void *user) {
    auto *tc = static_cast<TerminalCore *>(user);
    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        tc->sinkCursor(tc->cursorRow(), tc->cursorCol(), val->boolean != 0);
        break;
    case VTERM_PROP_TITLE:
        tc->sinkTitleFragment(val->string.str, static_cast<int>(val->string.len),
                              val->string.initial, val->string.final);
        break;
    case VTERM_PROP_ALTSCREEN:
        tc->sinkAltScreen(val->boolean != 0);
        break;
    default:
        break;
    }
    return 1;
}

int bellCb(void *user) {
    static_cast<TerminalCore *>(user)->sinkBell();
    return 1;
}

int sbPushLineCb(int cols, const VTermScreenCell *cells, void *user) {
    static_cast<TerminalCore *>(user)->sinkPushLine(cols, cells);
    return 1;
}

int sbPopLineCb(int cols, VTermScreenCell *cells, void *user) {
    return static_cast<TerminalCore *>(user)->sinkPopLine(cols, cells);
}

int sbClearCb(void *user) {
    static_cast<TerminalCore *>(user)->sinkSbClear();
    return 1;
}

const VTermScreenCallbacks kScreenCallbacks = {
    /* damage      */ damageCb,
    /* moverect    */ nullptr,
    /* movecursor  */ moveCursorCb,
    /* settermprop */ setTermPropCb,
    /* bell        */ bellCb,
    /* resize      */ nullptr,
    /* sb_pushline */ sbPushLineCb, // 滚出屏幕的行 → 历史缓冲
    /* sb_popline  */ sbPopLineCb,  // 屏幕变高 / reflow 时向历史索回
    /* sb_clear    */ sbClearCb,
};

// VTermScreenCell → Cell 快照转换（cellAt / historyCellAt 共用）
TerminalCore::Cell cellFromVTerm(VTermScreen *screen, const VTermScreenCell &c) {
    TerminalCore::Cell out;
    int n = 0;
    while (n < VTERM_MAX_CHARS_PER_CELL && c.chars[n] != 0)
        ++n;
    if (n > 0)
        out.text = QString::fromUcs4(reinterpret_cast<const char32_t *>(c.chars), n);
    out.width     = c.width > 0 ? c.width : 1;
    out.bold      = c.attrs.bold;
    out.underline = c.attrs.underline != 0;
    out.italic    = c.attrs.italic;
    out.reverse   = c.attrs.reverse;

    VTermColor fg = c.fg, bg = c.bg;
    vterm_screen_convert_color_to_rgb(screen, &fg);
    vterm_screen_convert_color_to_rgb(screen, &bg);
    out.fg = QColor(fg.rgb.red, fg.rgb.green, fg.rgb.blue);
    out.bg = QColor(bg.rgb.red, bg.rgb.green, bg.rgb.blue);
    return out;
}

VTermModifier toVtMod(Qt::KeyboardModifiers m) {
    int r = VTERM_MOD_NONE;
    if (m & Qt::ShiftModifier)
        r |= VTERM_MOD_SHIFT;
    if (m & Qt::AltModifier)
        r |= VTERM_MOD_ALT;
    if (m & Qt::ControlModifier)
        r |= VTERM_MOD_CTRL;
    return static_cast<VTermModifier>(r);
}

VTermKey toVtKey(TerminalCore::SpecialKey k) {
    using SK = TerminalCore::SpecialKey;
    // F1..F12 连续，用公式（VTERM_KEY_FUNCTION 展开为 int，需显式转回枚举）
    if (k >= SK::F1 && k <= SK::F12) {
        int n = 1 + (static_cast<int>(k) - static_cast<int>(SK::F1));
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(n));
    }
    switch (k) {
    case SK::Enter:     return VTERM_KEY_ENTER;
    case SK::Tab:       return VTERM_KEY_TAB;
    case SK::Backspace: return VTERM_KEY_BACKSPACE;
    case SK::Escape:    return VTERM_KEY_ESCAPE;
    case SK::Up:        return VTERM_KEY_UP;
    case SK::Down:      return VTERM_KEY_DOWN;
    case SK::Left:      return VTERM_KEY_LEFT;
    case SK::Right:     return VTERM_KEY_RIGHT;
    case SK::Insert:    return VTERM_KEY_INS;
    case SK::Delete:    return VTERM_KEY_DEL;
    case SK::Home:      return VTERM_KEY_HOME;
    case SK::End:       return VTERM_KEY_END;
    case SK::PageUp:    return VTERM_KEY_PAGEUP;
    case SK::PageDown:  return VTERM_KEY_PAGEDOWN;
    default:           return VTERM_KEY_NONE;
    }
}

} // namespace

// ── TerminalCore ────────────────────────────────────────────────────

TerminalCore::TerminalCore(int rows, int cols, QObject *parent)
    : QObject(parent), m_rows(rows), m_cols(cols), m_sb(std::make_unique<Scrollback>()) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, outputCb, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1); // 让 vim/htop 等能用备用屏
    vterm_screen_enable_reflow(m_screen, true); // resize 时长行重排（配合 sb_pop/push 往返）
    vterm_screen_reset(m_screen, 1);
}

TerminalCore::~TerminalCore() {
    if (m_vt)
        vterm_free(m_vt);
}

void TerminalCore::feed(const QByteArray &bytes) {
    if (m_vt && !bytes.isEmpty())
        vterm_input_write(m_vt, bytes.constData(), static_cast<size_t>(bytes.size()));
}

void TerminalCore::setSize(int rows, int cols) {
    if (!m_vt || rows <= 0 || cols <= 0)
        return;
    if (rows == m_rows && cols == m_cols)
        return;
    m_rows = rows;
    m_cols = cols;
    vterm_set_size(m_vt, rows, cols);
}

void TerminalCore::keyChar(uint32_t codepoint, Qt::KeyboardModifiers mods) {
    if (m_vt)
        vterm_keyboard_unichar(m_vt, codepoint, toVtMod(mods));
}

void TerminalCore::keySpecial(SpecialKey key, Qt::KeyboardModifiers mods) {
    if (m_vt)
        vterm_keyboard_key(m_vt, toVtKey(key), toVtMod(mods));
}

void TerminalCore::pasteText(const QString &text) {
    if (!m_vt || text.isEmpty())
        return;
    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\r"))
        .replace(QLatin1Char('\n'), QLatin1Char('\r')); // 终端换行统一为 CR
    vterm_keyboard_start_paste(m_vt); // 应用开了 bracketed paste 模式则自动包裹
    for (char32_t cp : t.toUcs4())
        vterm_keyboard_unichar(m_vt, cp, VTERM_MOD_NONE);
    vterm_keyboard_end_paste(m_vt);
}

TerminalCore::Cell TerminalCore::cellAt(int row, int col) const {
    if (!m_screen)
        return {};
    VTermPos        pos{row, col};
    VTermScreenCell c;
    if (!vterm_screen_get_cell(m_screen, pos, &c))
        return {};
    return cellFromVTerm(m_screen, c);
}

int TerminalCore::historySize() const {
    return m_sb ? static_cast<int>(m_sb->lines.size()) : 0;
}

qint64 TerminalCore::historyStart() const {
    return m_sb ? m_sb->evicted : 0;
}

TerminalCore::Cell TerminalCore::historyCellAt(int histRow, int col) const {
    if (!m_screen || !m_sb || histRow < 0 ||
        histRow >= static_cast<int>(m_sb->lines.size()))
        return {};
    const auto &cells = m_sb->lines[static_cast<size_t>(histRow)].cells;
    if (col < 0 || col >= cells.size())
        return {}; // 历史行比当前屏幕窄：余量按空白格处理
    return cellFromVTerm(m_screen, cells[col]);
}

void TerminalCore::setDefaultColors(const QColor &fg, const QColor &bg) {
    if (!m_screen)
        return;
    VTermColor vfg, vbg;
    vterm_color_rgb(&vfg, fg.red(), fg.green(), fg.blue());
    vterm_color_rgb(&vbg, bg.red(), bg.green(), bg.blue());
    vterm_screen_set_default_colors(m_screen, &vfg, &vbg);
}

// ── sink：从 C 回调触达，负责更新状态并 emit ────────────────────────

void TerminalCore::sinkOutput(const char *s, int len) {
    emit outputToPty(QByteArray(s, len));
}

void TerminalCore::sinkDamage(int row, int col, int height, int width) {
    emit damaged(row, col, height, width);
}

void TerminalCore::sinkCursor(int row, int col, bool visible) {
    m_cursorRow     = row;
    m_cursorCol     = col;
    m_cursorVisible = visible;
    emit cursorMoved(row, col, visible);
}

void TerminalCore::sinkBell() { emit bell(); }

// 注意：push/pop/clear 由 vterm 在 feed/resize 内部同步回调（reflow 下 resize 会
// 反复 push/pop 往返）。此处只动 deque + emit 轻量信号（槽内仅钳制偏移 + update），
// 严禁在槽里写回 vterm（feed/keyChar 等），否则重入。
void TerminalCore::sinkPushLine(int cols, const void *cellsPtr) {
    if (!m_sb)
        return;
    const auto *cells = static_cast<const VTermScreenCell *>(cellsPtr);
    Scrollback::Line line;
    line.cells.resize(cols);
    std::copy(cells, cells + cols, line.cells.begin());
    m_sb->lines.push_back(std::move(line));
    if (static_cast<int>(m_sb->lines.size()) > Scrollback::kMaxLines) {
        m_sb->lines.pop_front();
        ++m_sb->evicted;
    }
    emit scrollbackChanged(+1);
}

int TerminalCore::sinkPopLine(int cols, void *cellsPtr) {
    if (!m_sb || m_sb->lines.empty())
        return 0;
    auto       *cells = static_cast<VTermScreenCell *>(cellsPtr);
    const auto &line  = m_sb->lines.back().cells;
    const int   n     = std::min(cols, static_cast<int>(line.size()));
    std::copy(line.begin(), line.begin() + n, cells);
    // 历史行比请求窄：余量填默认色空白格
    VTermScreenCell blank;
    std::memset(&blank, 0, sizeof(blank));
    blank.width   = 1;
    blank.fg.type = VTERM_COLOR_DEFAULT_FG;
    blank.bg.type = VTERM_COLOR_DEFAULT_BG;
    for (int i = n; i < cols; ++i)
        cells[i] = blank;
    m_sb->lines.pop_back();
    emit scrollbackChanged(-1);
    return 1;
}

void TerminalCore::sinkSbClear() {
    if (!m_sb || m_sb->lines.empty())
        return;
    const int n = static_cast<int>(m_sb->lines.size());
    m_sb->lines.clear();
    emit scrollbackChanged(-n);
}

void TerminalCore::sinkAltScreen(bool on) { m_altScreen = on; }

void TerminalCore::sinkTitleFragment(const char *s, int len, bool initial, bool final) {
    if (initial)
        m_titleBuf.clear();
    m_titleBuf.append(QString::fromUtf8(s, len));
    if (final)
        emit titleChanged(m_titleBuf);
}

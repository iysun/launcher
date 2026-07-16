#include "terminalcore.h"

extern "C" {
#include "vterm.h"
}

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
    default:
        break;
    }
    return 1;
}

int bellCb(void *user) {
    static_cast<TerminalCore *>(user)->sinkBell();
    return 1;
}

const VTermScreenCallbacks kScreenCallbacks = {
    /* damage      */ damageCb,
    /* moverect    */ nullptr,
    /* movecursor  */ moveCursorCb,
    /* settermprop */ setTermPropCb,
    /* bell        */ bellCb,
    /* resize      */ nullptr,
    /* sb_pushline */ nullptr, // MVP：不做滚动回看
    /* sb_popline  */ nullptr,
    /* sb_clear    */ nullptr,
};

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
    : QObject(parent), m_rows(rows), m_cols(cols) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, outputCb, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1); // 让 vim/htop 等能用备用屏
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

TerminalCore::Cell TerminalCore::cellAt(int row, int col) const {
    Cell out;
    if (!m_screen)
        return out;
    VTermPos        pos{row, col};
    VTermScreenCell c;
    if (!vterm_screen_get_cell(m_screen, pos, &c))
        return out;

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
    vterm_screen_convert_color_to_rgb(m_screen, &fg);
    vterm_screen_convert_color_to_rgb(m_screen, &bg);
    out.fg = QColor(fg.rgb.red, fg.rgb.green, fg.rgb.blue);
    out.bg = QColor(bg.rgb.red, bg.rgb.green, bg.rgb.blue);
    return out;
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

void TerminalCore::sinkTitleFragment(const char *s, int len, bool initial, bool final) {
    if (initial)
        m_titleBuf.clear();
    m_titleBuf.append(QString::fromUtf8(s, len));
    if (final)
        emit titleChanged(m_titleBuf);
}

#include "terminalview.h"
#include "core/theme.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
// Qt::Key → TerminalCore::SpecialKey；返回 true 表示是需特殊编码的键
bool mapSpecial(int key, TerminalCore::SpecialKey &out) {
    using SK = TerminalCore::SpecialKey;
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    out = SK::Enter;     return true;
    case Qt::Key_Backspace: out = SK::Backspace; return true;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:  out = SK::Tab;       return true;
    case Qt::Key_Escape:   out = SK::Escape;    return true;
    case Qt::Key_Up:       out = SK::Up;        return true;
    case Qt::Key_Down:     out = SK::Down;      return true;
    case Qt::Key_Left:     out = SK::Left;      return true;
    case Qt::Key_Right:    out = SK::Right;     return true;
    case Qt::Key_Insert:   out = SK::Insert;    return true;
    case Qt::Key_Delete:   out = SK::Delete;    return true;
    case Qt::Key_Home:     out = SK::Home;      return true;
    case Qt::Key_End:      out = SK::End;       return true;
    case Qt::Key_PageUp:   out = SK::PageUp;    return true;
    case Qt::Key_PageDown: out = SK::PageDown;  return true;
    case Qt::Key_F1:  out = SK::F1;  return true;
    case Qt::Key_F2:  out = SK::F2;  return true;
    case Qt::Key_F3:  out = SK::F3;  return true;
    case Qt::Key_F4:  out = SK::F4;  return true;
    case Qt::Key_F5:  out = SK::F5;  return true;
    case Qt::Key_F6:  out = SK::F6;  return true;
    case Qt::Key_F7:  out = SK::F7;  return true;
    case Qt::Key_F8:  out = SK::F8;  return true;
    case Qt::Key_F9:  out = SK::F9;  return true;
    case Qt::Key_F10: out = SK::F10; return true;
    case Qt::Key_F11: out = SK::F11; return true;
    case Qt::Key_F12: out = SK::F12; return true;
    default: return false;
    }
}
} // namespace

TerminalView::TerminalView(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);    // 我们自己铺满背景，省一次系统擦除
    setAttribute(Qt::WA_InputMethodEnabled); // 中文等 IME 输入（inputMethodEvent）
    setCursor(Qt::IBeamCursor);

    // 等宽字体：优先 Cascadia Mono / Consolas，兜底系统固定字体
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (const QString &fam : {QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")}) {
        if (QFontDatabase::families().contains(fam)) {
            m_font = QFont(fam);
            break;
        }
    }
    m_font.setPointSize(11);
    m_font.setFixedPitch(true);

    QFontMetricsF fm(m_font);
    m_cellW = std::ceil(fm.horizontalAdvance(QLatin1Char('M')));
    m_cellH = std::ceil(fm.height());

    m_defaultFg = Theme::color("text");
    m_defaultBg = Theme::color("bg");

    m_core = new TerminalCore(m_rows, m_cols, this);
    m_core->setDefaultColors(m_defaultFg, m_defaultBg);
    m_core->applyAnsiPalette(Theme::ansiPalette()); // ANSI 16 色跟主题（启动一次）

    connect(m_core, &TerminalCore::outputToPty, this, &TerminalView::outputToPty);
    connect(m_core, &TerminalCore::damaged, this, [this] { update(); });
    connect(m_core, &TerminalCore::cursorMoved, this, [this] {
        updateMicroFocus(); // 通知输入法光标矩形变化（候选框跟随）
        update();
    });
    // 历史行数变化：上滚期间随内容平移偏移量（视口内容保持稳定），并钳制越界
    connect(m_core, &TerminalCore::scrollbackChanged, this, [this](int delta) {
        if (m_scrollOffset > 0)
            m_scrollOffset = std::clamp(m_scrollOffset + delta, 0, m_core->historySize());
        update();
    });
    // 应用鼠标追踪模式变化：Move 模式需要即使未按键也收到 mouseMoveEvent
    connect(m_core, &TerminalCore::mouseModeChanged, this, [this](TerminalCore::MouseMode m) {
        m_mouseMode = m;
        setMouseTracking(m == TerminalCore::MouseMode::Move);
        if (m != TerminalCore::MouseMode::None)
            clearSelection(); // 应用接管鼠标，清掉遗留选区
    });
}

void TerminalView::onBytes(const QByteArray &chunk) {
    m_core->feed(chunk); // feed 内部触发 damaged → update()
}

void TerminalView::recomputeGrid() {
    int cols = std::max(1, int(width() / m_cellW));
    int rows = std::max(1, int(height() / m_cellH));
    if (cols == m_cols && rows == m_rows)
        return;
    m_cols = cols;
    m_rows = rows;
    m_core->setSize(rows, cols);
    emit resized(rows, cols);
}

void TerminalView::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    recomputeGrid();
}

TerminalCore::Cell TerminalView::fetchCell(int viewRow, int col) const {
    // 全序列 = 历史行 [0, hist) + 屏幕行 [hist, hist+m_rows)；偏移从底端往回数
    const int seq = m_core->historySize() - m_scrollOffset + viewRow;
    if (seq < 0)
        return {};
    if (seq < m_core->historySize())
        return m_core->historyCellAt(seq, col);
    return m_core->cellAt(seq - m_core->historySize(), col);
}

void TerminalView::scrollTo(int offset) {
    const int clamped = std::clamp(offset, 0, m_core->historySize());
    if (clamped == m_scrollOffset)
        return;
    m_scrollOffset = clamped;
    update();
}

// ── 选区与复制粘贴 ──────────────────────────────────────────────────

qint64 TerminalView::absRowOfViewRow(int viewRow) const {
    // 全局行号 = 驱逐计数 + 本地序号（历史 [0,hist) + 屏幕 [hist,hist+rows)）
    return m_core->historyStart() + m_core->historySize() - m_scrollOffset + viewRow;
}

TerminalView::SelPos TerminalView::posFromPixel(const QPointF &p) const {
    const int viewRow = std::clamp(int(p.y() / m_cellH), 0, m_rows - 1);
    const int col     = std::clamp(int(p.x() / m_cellW), 0, m_cols - 1);
    return {absRowOfViewRow(viewRow), col};
}

TerminalCore::Cell TerminalView::cellAtAbs(qint64 absRow, int col) const {
    const qint64 seq = absRow - m_core->historyStart(); // 本地序号
    if (seq < 0)
        return {};
    if (seq < m_core->historySize())
        return m_core->historyCellAt(static_cast<int>(seq), col);
    const qint64 screenRow = seq - m_core->historySize();
    if (screenRow >= m_core->rows())
        return {};
    return m_core->cellAt(static_cast<int>(screenRow), col);
}

bool TerminalView::selectionContains(qint64 absRow, int col) const {
    if (!m_hasSel)
        return false;
    SelPos a = m_selAnchor, b = m_selEnd;
    if (b < a)
        std::swap(a, b);
    if (absRow < a.row || absRow > b.row)
        return false;
    if (absRow == a.row && col < a.col)
        return false;
    if (absRow == b.row && col > b.col)
        return false;
    return true;
}

QString TerminalView::selectedText() const {
    if (!m_hasSel)
        return {};
    SelPos a = m_selAnchor, b = m_selEnd;
    if (b < a)
        std::swap(a, b);
    QStringList lines;
    for (qint64 row = a.row; row <= b.row; ++row) {
        const int cFrom = (row == a.row) ? a.col : 0;
        const int cTo   = (row == b.row) ? b.col : m_cols - 1;
        QString   line;
        for (int col = cFrom; col <= cTo;) {
            TerminalCore::Cell cell = cellAtAbs(row, col);
            line += cell.text.isEmpty() ? QStringLiteral(" ") : cell.text;
            col += cell.width > 1 ? 2 : 1; // 宽字符跳过延续格
        }
        while (line.endsWith(QLatin1Char(' ')))
            line.chop(1); // 行尾空白裁剪
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

void TerminalView::clearSelection() {
    m_selecting = false;
    if (m_hasSel) {
        m_hasSel = false;
        update();
    }
}

void TerminalView::copySelection() {
    const QString t = selectedText();
    if (!t.isEmpty())
        QGuiApplication::clipboard()->setText(t);
}

void TerminalView::pasteClipboard() {
    const QString t = QGuiApplication::clipboard()->text();
    if (t.isEmpty())
        return;
    clearSelection();
    scrollTo(0);
    m_core->pasteText(t); // 编码经 outputToPty 信号写回 PTY
}

QPoint TerminalView::mouseCellOf(const QPointF &p) const {
    const int col = std::clamp(int(p.x() / m_cellW), 0, m_cols - 1);
    const int row = std::clamp(int(p.y() / m_cellH), 0, m_rows - 1);
    return {col, row};
}

bool TerminalView::forwardMouseToApp(Qt::KeyboardModifiers mods) const {
    // 应用开了鼠标模式且未按 Shift 时转发；Shift 强制本地选区（xterm/foot 惯例）
    return m_mouseMode != TerminalCore::MouseMode::None && !(mods & Qt::ShiftModifier);
}

namespace {
// Qt 鼠标键 → libvterm 按钮号（1=左 2=中 3=右）
int vtMouseButton(Qt::MouseButton b) {
    switch (b) {
    case Qt::LeftButton:   return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton:  return 3;
    default:               return 0;
    }
}
} // namespace

void TerminalView::mousePressEvent(QMouseEvent *e) {
    if (forwardMouseToApp(e->modifiers())) {
        const int btn = vtMouseButton(e->button());
        if (btn) {
            const QPoint c = mouseCellOf(e->position());
            m_core->mouseMove(c.y(), c.x(), e->modifiers());
            m_core->mouseButton(btn, true, e->modifiers());
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton) {
        clearSelection();
        m_selAnchor = m_selEnd = posFromPixel(e->position());
        m_selecting = true;
        e->accept();
        return;
    }
    if (e->button() == Qt::RightButton) { // 右键 = 粘贴（Windows 终端惯例）
        pasteClipboard();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TerminalView::mouseMoveEvent(QMouseEvent *e) {
    if (forwardMouseToApp(e->modifiers()) && !m_selecting) {
        // Drag 模式只在按键期间上报移动；Move 模式任意移动都上报
        const bool anyButton = e->buttons() != Qt::NoButton;
        if (m_mouseMode == TerminalCore::MouseMode::Move ||
            (m_mouseMode == TerminalCore::MouseMode::Drag && anyButton)) {
            const QPoint c = mouseCellOf(e->position());
            if (c != m_lastReportedCell) {
                m_lastReportedCell = c;
                m_core->mouseMove(c.y(), c.x(), e->modifiers());
            }
        }
        e->accept();
        return;
    }
    if (m_selecting) {
        const SelPos pos = posFromPixel(e->position());
        if (!(pos == m_selEnd)) {
            m_selEnd = pos;
            m_hasSel = !(m_selAnchor == m_selEnd);
            update();
        }
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void TerminalView::mouseReleaseEvent(QMouseEvent *e) {
    // 转发释放：即便本次未按 Shift，只要不是正在本地选区，就上报给应用
    if (m_mouseMode != TerminalCore::MouseMode::None && !m_selecting) {
        const int btn = vtMouseButton(e->button());
        if (btn) {
            const QPoint c = mouseCellOf(e->position());
            m_core->mouseMove(c.y(), c.x(), e->modifiers());
            m_core->mouseButton(btn, false, e->modifiers());
            e->accept();
            return;
        }
    }
    if (e->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void TerminalView::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(e);
        return;
    }
    // 双击选词：以点击格为中心向两侧扩展「词字符」。分隔符集合含空白与常见标点，但
    // 保留 / . - _ ~ 等为词内，让路径 / 域名 / kebab 命名能整体选中（终端用户预期）。
    const SelPos hit        = posFromPixel(e->position());
    auto         isWordChar = [this, &hit](int col) {
        const QString t = cellAtAbs(hit.row, col).text;
        if (t.isEmpty())
            return false;
        const QChar ch = t.at(0);
        if (ch.isSpace())
            return false;
        static const QString kDelims = QStringLiteral("`~!@#$%^&*()=+[]{}\\|;:'\",<>?");
        return !kDelims.contains(ch);
    };
    if (!isWordChar(hit.col)) {
        e->accept();
        return;
    }
    int from = hit.col, to = hit.col;
    while (from > 0 && isWordChar(from - 1))
        --from;
    while (to < m_cols - 1 && isWordChar(to + 1))
        ++to;
    m_selAnchor = {hit.row, from};
    m_selEnd    = {hit.row, to};
    m_hasSel    = true;
    update();
    e->accept();
}

void TerminalView::wheelEvent(QWheelEvent *e) {
    const int steps = e->angleDelta().y() / 120; // 正 = 向上
    if (steps == 0 || !m_core) {
        e->ignore();
        return;
    }
    // 应用鼠标模式（未按 Shift）：滚轮上报为 button 4（上）/ 5（下）
    if (forwardMouseToApp(e->modifiers())) {
        const int    btn = steps > 0 ? 4 : 5;
        const QPoint c   = mouseCellOf(e->position());
        m_core->mouseMove(c.y(), c.x(), e->modifiers()); // 先定位，button 复用当前坐标
        for (int i = 0; i < qAbs(steps); ++i) {
            m_core->mouseButton(btn, true, e->modifiers());
            m_core->mouseButton(btn, false, e->modifiers());
        }
        e->accept();
        return;
    }
    if (m_core->altScreen()) {
        // 备用屏（vim/less/htop）无 scrollback：滚轮转发为方向键，3 行/格
        const auto key = steps > 0 ? TerminalCore::SpecialKey::Up
                                   : TerminalCore::SpecialKey::Down;
        for (int i = 0; i < qAbs(steps) * 3; ++i)
            m_core->keySpecial(key, Qt::NoModifier);
    } else {
        scrollTo(m_scrollOffset + steps * 3);
    }
    e->accept();
}

void TerminalView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), m_defaultBg);

    QColor selBg = Theme::color("accent");
    selBg.setAlpha(70);

    for (int row = 0; row < m_rows; ++row) {
        const qint64 absRow = absRowOfViewRow(row);
        for (int col = 0; col < m_cols;) {
            TerminalCore::Cell cell = fetchCell(row, col);
            int    w  = cell.width > 1 ? 2 : 1;
            qreal  x  = col * m_cellW;
            qreal  y  = row * m_cellH;
            QRectF r(x, y, m_cellW * w, m_cellH);

            QColor fg = cell.fg.isValid() ? cell.fg : m_defaultFg;
            QColor bg = cell.bg.isValid() ? cell.bg : m_defaultBg;
            if (cell.reverse)
                std::swap(fg, bg);

            if (bg != m_defaultBg)
                p.fillRect(r, bg);

            if (!cell.text.isEmpty()) {
                QFont f = m_font;
                f.setBold(cell.bold);
                f.setItalic(cell.italic);
                f.setUnderline(cell.underline);
                p.setFont(f);
                p.setPen(fg);
                p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, cell.text);
            }
            if (m_hasSel && selectionContains(absRow, col))
                p.fillRect(r, selBg); // 选区半透明覆盖（文字之上，轻微染色可接受）
            col += w;
        }
    }

    // 光标（块状，MVP）；回看历史时光标不在视口内，不画
    if (m_scrollOffset == 0 && m_core->cursorVisible()) {
        qreal x = m_core->cursorCol() * m_cellW;
        qreal y = m_core->cursorRow() * m_cellH;
        QRectF cr(x, y, m_cellW, m_cellH);
        QColor cur = Theme::color("accent");
        cur.setAlpha(140);
        p.fillRect(cr, cur);
    }

    // IME 预编辑串：画在光标处（覆盖其后格子，可接受）；候选未上屏不进 vterm 网格
    if (!m_preedit.isEmpty() && m_scrollOffset == 0) {
        QFontMetricsF fm(m_font);
        const qreal   w = fm.horizontalAdvance(m_preedit);
        const qreal   x = m_core->cursorCol() * m_cellW;
        const qreal   y = m_core->cursorRow() * m_cellH;
        QRectF        r(x, y, w, m_cellH);
        QColor        bg = Theme::color("accent");
        bg.setAlpha(60);
        p.fillRect(r, bg);
        QFont f = m_font;
        f.setUnderline(true);
        p.setFont(f);
        p.setPen(m_defaultFg);
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, m_preedit);
    }

    // 回看时右侧细条位置指示（非交互，代替滚动条）
    if (m_scrollOffset > 0) {
        const qreal total   = m_core->historySize() + m_rows;
        const qreal barH    = std::max<qreal>(20.0, height() * (m_rows / total));
        const qreal topFrac = (m_core->historySize() - m_scrollOffset) / total;
        const qreal top     = std::min<qreal>(height() * topFrac, height() - barH);
        QColor      c       = Theme::color("accent");
        c.setAlpha(120);
        p.fillRect(QRectF(width() - 5, top, 3, barH), c);
    }
}

void TerminalView::keyPressEvent(QKeyEvent *e) {
    if (!m_core) {
        QWidget::keyPressEvent(e);
        return;
    }
    Qt::KeyboardModifiers mods = e->modifiers();

    // 复制/粘贴快捷键：须在 Ctrl+字母 分支之前拦截，否则被编码成控制字节吞掉
    if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
        if (e->key() == Qt::Key_C && m_hasSel) { // 无选区时放行（仍作 SIGINT）
            copySelection();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_V) {
            pasteClipboard();
            e->accept();
            return;
        }
    }

    // Shift+翻页/Home/End：本地回看 scrollback，不下发给应用（经典终端行为）；
    // 须在 mapSpecial 之前拦截，否则这些键会被编码发给应用。
    if (mods & Qt::ShiftModifier) {
        const int page = std::max(1, m_rows - 1); // 翻一屏，留一行重叠
        switch (e->key()) {
        case Qt::Key_PageUp:   scrollTo(m_scrollOffset + page);    e->accept(); return;
        case Qt::Key_PageDown: scrollTo(m_scrollOffset - page);    e->accept(); return;
        case Qt::Key_Home:     scrollTo(m_core->historySize());    e->accept(); return; // 跳到最顶
        case Qt::Key_End:      scrollTo(0);                        e->accept(); return; // 贴回底部
        default: break;
        }
    }

    TerminalCore::SpecialKey sk;
    if (mapSpecial(e->key(), sk)) {
        scrollTo(0); // 写 PTY 的按键先贴底（经典终端行为）
        clearSelection();
        m_core->keySpecial(sk, mods);
        e->accept();
        return;
    }

    // Ctrl+字母：传基字符 + CTRL，让 libvterm 生成控制字节（Ctrl+C=0x03 等）
    if ((mods & Qt::ControlModifier) && e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) {
        scrollTo(0);
        clearSelection();
        m_core->keyChar('a' + (e->key() - Qt::Key_A), mods);
        e->accept();
        return;
    }

    // 其余控制字符：Ctrl+[ 期望等价 Esc(0x1b)——但 libvterm 对 '[' 走 CSI u 编码
    // （原样交给 keyChar 会得到 ESC[91;5u，vanilla vim 不认），故直接发无修饰的
    // Escape。Ctrl+\ ] ^ _ 与 Ctrl+Space 则由 libvterm 的 c&0x1f 正确生成控制字节。
    if (mods & Qt::ControlModifier) {
        int  base  = 0;      // 送 keyChar 的基字符；0 = 不处理
        bool asEsc = false;
        switch (e->key()) {
        case Qt::Key_BracketLeft:  asEsc = true; break; // Ctrl+[ → ESC 0x1b
        case Qt::Key_Backslash:    base = '\\'; break;  // → 0x1c
        case Qt::Key_BracketRight: base = ']';  break;  // → 0x1d
        case Qt::Key_AsciiCircum:  base = '^';  break;  // → 0x1e
        case Qt::Key_Underscore:   base = '_';  break;  // → 0x1f
        case Qt::Key_Space:        base = ' ';  break;  // Ctrl+Space → 0x00
        default: break;
        }
        if (asEsc) {
            scrollTo(0);
            clearSelection();
            m_core->keySpecial(TerminalCore::SpecialKey::Escape, Qt::NoModifier);
            e->accept();
            return;
        }
        if (base) {
            scrollTo(0);
            clearSelection();
            m_core->keyChar(static_cast<uint32_t>(base), Qt::ControlModifier);
            e->accept();
            return;
        }
    }

    const QString t = e->text();
    if (!t.isEmpty()) {
        scrollTo(0);
        clearSelection();
        // 可打印字符已含 Shift 效果，去掉 SHIFT 修饰避免二次处理
        Qt::KeyboardModifiers cm = mods & ~Qt::ShiftModifier;
        for (char32_t cp : t.toUcs4())
            m_core->keyChar(cp, cm);
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

// ── IME（中文等输入法）─────────────────────────────────────────────

void TerminalView::inputMethodEvent(QInputMethodEvent *e) {
    const QString &commit = e->commitString();
    if (!commit.isEmpty()) {
        scrollTo(0);
        clearSelection();
        for (char32_t cp : commit.toUcs4())
            m_core->keyChar(cp, Qt::NoModifier);
    }
    if (m_preedit != e->preeditString()) {
        m_preedit = e->preeditString();
        update();
    }
    e->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery q) const {
    switch (q) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle: // 候选框定位：光标所在格（回看时光标在底部之外，给底部）
        if (m_scrollOffset == 0)
            return QRectF(m_core->cursorCol() * m_cellW, m_core->cursorRow() * m_cellH,
                          m_cellW, m_cellH);
        return QRectF(0, height() - m_cellH, m_cellW, m_cellH);
    case Qt::ImFont:
        return m_font;
    case Qt::ImHints:
        return int(Qt::ImhNone);
    default:
        return QWidget::inputMethodQuery(q);
    }
}

void TerminalView::focusOutEvent(QFocusEvent *e) {
    if (!m_preedit.isEmpty()) {
        m_preedit.clear();
        update();
    }
    QWidget::focusOutEvent(e);
}

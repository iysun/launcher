#include "terminalview.h"
#include "core/theme.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
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
    setAttribute(Qt::WA_OpaquePaintEvent); // 我们自己铺满背景，省一次系统擦除

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

    connect(m_core, &TerminalCore::outputToPty, this, &TerminalView::outputToPty);
    connect(m_core, &TerminalCore::damaged, this, [this] { update(); });
    connect(m_core, &TerminalCore::cursorMoved, this, [this] { update(); });
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

void TerminalView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), m_defaultBg);

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols;) {
            TerminalCore::Cell cell = m_core->cellAt(row, col);
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
            col += w;
        }
    }

    // 光标（块状，MVP）
    if (m_core->cursorVisible()) {
        qreal x = m_core->cursorCol() * m_cellW;
        qreal y = m_core->cursorRow() * m_cellH;
        QRectF cr(x, y, m_cellW, m_cellH);
        QColor cur = Theme::color("accent");
        cur.setAlpha(140);
        p.fillRect(cr, cur);
    }
}

void TerminalView::keyPressEvent(QKeyEvent *e) {
    if (!m_core) {
        QWidget::keyPressEvent(e);
        return;
    }
    Qt::KeyboardModifiers mods = e->modifiers();

    TerminalCore::SpecialKey sk;
    if (mapSpecial(e->key(), sk)) {
        m_core->keySpecial(sk, mods);
        e->accept();
        return;
    }

    // Ctrl+字母：传基字符 + CTRL，让 libvterm 生成控制字节（Ctrl+C=0x03 等）
    if ((mods & Qt::ControlModifier) && e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) {
        m_core->keyChar('a' + (e->key() - Qt::Key_A), mods);
        e->accept();
        return;
    }

    const QString t = e->text();
    if (!t.isEmpty()) {
        // 可打印字符已含 Shift 效果，去掉 SHIFT 修饰避免二次处理
        Qt::KeyboardModifiers cm = mods & ~Qt::ShiftModifier;
        for (char32_t cp : t.toUcs4())
            m_core->keyChar(cp, cm);
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

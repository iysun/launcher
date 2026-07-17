#pragma once
#include "vt/terminalcore.h"
#include <QFont>
#include <QPoint>
#include <QWidget>

// 终端渲染 + 输入控件：持有 TerminalCore（libvterm），用 QPainter 逐格自绘，
// keyPressEvent 把 Qt 按键交给 TerminalCore 编码。尺寸由字体度量换算成 rows/cols。
class TerminalView : public QWidget {
    Q_OBJECT
public:
    explicit TerminalView(QWidget *parent = nullptr);

    TerminalCore *core() const { return m_core; }
    int           gridRows() const { return m_rows; }
    int           gridCols() const { return m_cols; }

public slots:
    void onBytes(const QByteArray &chunk); // ← PtyReader（Queued）

signals:
    void outputToPty(const QByteArray &bytes); // 转发自 TerminalCore → ConPty::write
    void resized(int rows, int cols);          // 网格尺寸变化 → ConPty::resize

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    // 选区端点：row 为全局行号（historyStart + 历史/屏幕序号），滚动与驱逐下稳定
    struct SelPos {
        qint64 row = 0;
        int    col = 0;
        bool   operator==(const SelPos &o) const { return row == o.row && col == o.col; }
        bool   operator<(const SelPos &o) const {
            return row != o.row ? row < o.row : col < o.col;
        }
    };

    void recomputeGrid();
    // 视口取格：viewRow 0..m_rows-1，按 m_scrollOffset 分派历史行 / 屏幕行
    TerminalCore::Cell fetchCell(int viewRow, int col) const;
    void               scrollTo(int offset); // 钳制 [0, historySize] 后重绘

    qint64             absRowOfViewRow(int viewRow) const; // 视口行 → 全局行号
    SelPos             posFromPixel(const QPointF &p) const;
    TerminalCore::Cell cellAtAbs(qint64 absRow, int col) const;
    bool               selectionContains(qint64 absRow, int col) const;
    QString            selectedText() const;
    void               clearSelection();
    void               copySelection();
    void               pasteClipboard();

    // 鼠标上报：屏幕内 0-based 行列（应用鼠标模式基本在 altscreen，贴底）
    QPoint mouseCellOf(const QPointF &p) const;
    // 事件是否应转发给应用（应用开了鼠标模式且未按 Shift）；Shift=强制本地选区
    bool   forwardMouseToApp(Qt::KeyboardModifiers mods) const;

    QColor m_defaultFg, m_defaultBg;

    TerminalCore *m_core = nullptr;
    QFont         m_font;
    qreal         m_cellW = 8, m_cellH = 16;
    int           m_rows = 24, m_cols = 80;
    int           m_scrollOffset = 0; // 0 = 贴底；单位 = 距底行数

    bool   m_hasSel = false, m_selecting = false;
    SelPos m_selAnchor, m_selEnd; // 端点含入（end 指向格子本身）

    TerminalCore::MouseMode m_mouseMode = TerminalCore::MouseMode::None;
    QPoint                  m_lastReportedCell{-1, -1}; // 去重连续 mouseMove 上报

    QString m_preedit; // IME 预编辑串（候选未上屏），画在光标处
};

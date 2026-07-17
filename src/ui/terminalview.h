#pragma once
#include "vt/terminalcore.h"
#include <QFont>
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

    QColor m_defaultFg, m_defaultBg;

    TerminalCore *m_core = nullptr;
    QFont         m_font;
    qreal         m_cellW = 8, m_cellH = 16;
    int           m_rows = 24, m_cols = 80;
    int           m_scrollOffset = 0; // 0 = 贴底；单位 = 距底行数

    bool   m_hasSel = false, m_selecting = false;
    SelPos m_selAnchor, m_selEnd; // 端点含入（end 指向格子本身）
};

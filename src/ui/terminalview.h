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

private:
    void   recomputeGrid();
    QColor m_defaultFg, m_defaultBg;

    TerminalCore *m_core = nullptr;
    QFont         m_font;
    qreal         m_cellW = 8, m_cellH = 16;
    int           m_rows = 24, m_cols = 80;
};

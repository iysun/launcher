#include "terminalwindow.h"
#include "core/theme.h"
#include "ui/terminaltabs.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

TerminalWindow::TerminalWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("Terminal"));
    // 无系统标题栏；保留最小化/最大化/关闭能力（任务栏、Aero snap）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint |
                   Qt::WindowCloseButtonHint);
    resize(900, 560);

    const QString text = Theme::c("text"), hover = Theme::c("hoverBg"),
                  danger = Theme::c("danger");

    m_tabs = new TerminalTabs(this); // 原生方框区，无圆角遮罩
    m_tabs->setStripAlwaysVisible(true); // 标签栏兼作标题栏，常显
    connect(m_tabs, &TerminalTabs::lastTabClosed, this, &TerminalWindow::close);

    // ── 窗口控制键，塞进标签栏右侧（与标签二合一）──
    auto *winButtons = new QWidget;
    auto *wbLay      = new QHBoxLayout(winButtons);
    wbLay->setContentsMargins(0, 0, 0, 0);
    wbLay->setSpacing(0);

    const QString btnQss = QString("QToolButton{border:none;background:transparent;"
                                   "color:%1;font-size:13px;min-width:42px;min-height:30px;}"
                                   "QToolButton:hover{background:%2;}")
                               .arg(text, hover);
    auto *minBtn = new QToolButton(winButtons);
    minBtn->setText(QStringLiteral("—"));
    minBtn->setStyleSheet(btnQss);
    minBtn->setFocusPolicy(Qt::NoFocus);
    auto *maxBtn = new QToolButton(winButtons);
    maxBtn->setText(QStringLiteral("▢"));
    maxBtn->setStyleSheet(btnQss);
    maxBtn->setFocusPolicy(Qt::NoFocus);
    auto *closeBtn = new QToolButton(winButtons);
    closeBtn->setText(QStringLiteral("✕"));
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setStyleSheet(QString("QToolButton{border:none;background:transparent;"
                                    "color:%1;font-size:13px;min-width:42px;min-height:30px;}"
                                    "QToolButton:hover{background:%2;color:#ffffff;}")
                                .arg(text, danger));
    wbLay->addWidget(minBtn);
    wbLay->addWidget(maxBtn);
    wbLay->addWidget(closeBtn);

    connect(minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QToolButton::clicked, this, &TerminalWindow::toggleMaximize);
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::close);

    m_tabs->addStripTrailing(winButtons);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_tabs);

    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

void TerminalWindow::toggleMaximize() {
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void TerminalWindow::startSession() { m_tabs->startSession(); }

void TerminalWindow::runCommand(const QString &cmd) { m_tabs->runCommand(cmd); }

void TerminalWindow::closeEvent(QCloseEvent *e) {
    m_tabs->teardown();
    QWidget::closeEvent(e);
}

#ifdef Q_OS_WIN
bool TerminalWindow::nativeEvent(const QByteArray &eventType, void *message,
                                 qintptr *result) {
    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCHITTEST) {
        const int    bw = 6; // 边缘抓取宽度
        const QPoint g(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        const QPoint p = mapFromGlobal(g);
        const int    w = width(), h = height();

        if (!isMaximized()) { // 最大化时不做边缘缩放
            const bool L = p.x() < bw, R = p.x() >= w - bw;
            const bool T = p.y() < bw, B = p.y() >= h - bw;
            if (T && L) { *result = HTTOPLEFT; return true; }
            if (T && R) { *result = HTTOPRIGHT; return true; }
            if (B && L) { *result = HTBOTTOMLEFT; return true; }
            if (B && R) { *result = HTBOTTOMRIGHT; return true; }
            if (L) { *result = HTLEFT; return true; }
            if (R) { *result = HTRIGHT; return true; }
            if (T) { *result = HTTOP; return true; }
            if (B) { *result = HTBOTTOM; return true; }
        }
        // 只有标签栏里的空白拖拽区可拖动（标签/“+”/窗口按钮都保持 HTCLIENT 可点）
        if (m_tabs && m_tabs->pointInDragArea(g)) {
            *result = HTCAPTION;
            return true;
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

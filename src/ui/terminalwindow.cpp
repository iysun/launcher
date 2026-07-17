#include "terminalwindow.h"
#include "core/theme.h"
#include "ui/terminalpane.h"

#include <QCloseEvent>
#include <QVBoxLayout>

TerminalWindow::TerminalWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("Terminal"));
    resize(900, 520);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_pane = new TerminalPane(this);
    lay->addWidget(m_pane);

    // shell 退出时更新标题（会话已由 pane 内部 teardown）
    connect(m_pane, &TerminalPane::sessionEnded, this,
            [this] { setWindowTitle(QStringLiteral("Terminal (已退出)")); });

    // 窗口背景与终端一致
    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

void TerminalWindow::startSession() {
    m_pane->startSession();
    setWindowTitle(QStringLiteral("Terminal"));
}

void TerminalWindow::runCommand(const QString &cmd) { m_pane->runCommand(cmd); }

void TerminalWindow::closeEvent(QCloseEvent *e) {
    m_pane->teardown();
    QWidget::closeEvent(e);
}

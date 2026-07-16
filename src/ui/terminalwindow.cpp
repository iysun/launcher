#include "terminalwindow.h"
#include "core/theme.h"
#include "pty/conpty.h"
#include "pty/ptyreader.h"
#include "ui/terminalview.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
// 选 shell：pwsh(7) > powershell(5.1) > cmd
QString pickShell() {
    QString pwsh = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
    if (!pwsh.isEmpty())
        return pwsh;
    QString ps = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (!ps.isEmpty())
        return ps;
    return QStringLiteral("cmd.exe");
}
} // namespace

TerminalWindow::TerminalWindow(QWidget *parent)
    : QWidget(parent), m_pty(std::make_unique<ConPty>()) {
    setWindowTitle(QStringLiteral("Terminal"));
    resize(900, 520);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_view = new TerminalView(this);
    lay->addWidget(m_view);

    // m_view 与 m_pty 都随窗口生命周期长存，这两条连接**只连一次**（放构造函数）。
    // 若放在 startSession 里，重开会话会重复连接 → 每个键写多次（经典重复输入 bug）。
    // 键盘编码 / 查询响应 → 写回 PTY（无会话时 m_pty->write 内部直接返回，无害）
    connect(m_view, &TerminalView::outputToPty, this, [this](const QByteArray &b) {
        m_pty->write(b.constData(), b.size());
    });
    // 网格尺寸变化 → ConPTY resize（注意 ConPTY 参数是 cols,rows）
    connect(m_view, &TerminalView::resized, this, [this](int r, int c) {
        m_pty->resize(static_cast<short>(c), static_cast<short>(r));
    });

    // 窗口背景与终端一致
    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

TerminalWindow::~TerminalWindow() { teardown(); }

void TerminalWindow::startSession() {
    if (m_started)
        return;

    const int rows = m_view->gridRows();
    const int cols = m_view->gridCols();

    if (!m_pty->start(pickShell(), static_cast<short>(cols), static_cast<short>(rows))) {
        setWindowTitle(QStringLiteral("Terminal (启动失败)"));
        return;
    }

#ifdef Q_OS_WIN
    m_reader = new PtyReader(this);
    m_reader->setReadHandle(m_pty->readHandle());

    // PTY 输出 → 视图（跨线程 Queued，自动）。m_reader 每会话新建，故这些连接放这里。
    connect(m_reader, &PtyReader::bytesRead, m_view, &TerminalView::onBytes);
    connect(m_reader, &PtyReader::eof, this, &TerminalWindow::onSessionEnded);

    m_reader->start();
#endif

    m_started = true;
    m_view->setFocus();
    setWindowTitle(QStringLiteral("Terminal"));
}

void TerminalWindow::onSessionEnded() {
    setWindowTitle(QStringLiteral("Terminal (已退出)"));
    // 允许下次 /terminal 重新起会话
    teardown();
}

void TerminalWindow::closeEvent(QCloseEvent *e) {
    teardown();
    QWidget::closeEvent(e);
}

void TerminalWindow::teardown() {
    if (m_reader) {
        m_reader->requestStop();
        m_pty->stop();       // 关句柄，解锁阻塞的 ReadFile
        m_reader->wait(3000); // join
        m_reader->deleteLater();
        m_reader = nullptr;
    } else {
        m_pty->stop();
    }
    m_started = false;
}

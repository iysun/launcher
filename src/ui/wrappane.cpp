#include "wrappane.h"
#include "core/i18n.h"
#include "core/theme.h"
#include "pty/conpty.h"
#include "pty/ptyreader.h"
#include "ui/terminalview.h"

#include <QByteArray>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
// 常驻 shell 命令行：pwsh(7) > powershell(5.1) > cmd。-NoExit 保持会话常驻（命令经
// stdin 逐条送入，避免每条命令新起重进程）；-NoLogo 去横幅、-NoProfile 跳过 profile
// （无噪声、启动快）；-Command 把提示符设空，让输出干净（每条命令前另行清屏）。
// ConPTY 伪控制台让 pwsh 的 $PSStyle 默认 Host 渲染 → Get-ChildItem 等自动带 ANSI 颜色。
QString shellCommandLine() {
    QString exe = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
    if (exe.isEmpty())
        exe = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (!exe.isEmpty())
        return QString(QStringLiteral(
                   "\"%1\" -NoLogo -NoProfile -NoExit -Command \""
                   "if ($PSStyle) { $PSStyle.OutputRendering = 'Ansi' }; "
                   "function prompt { '' }\""))
            .arg(exe);
    return QStringLiteral("cmd.exe"); // cmd 无空提示符处理，回退时会显示 C:\..> 提示符
}

// 清 scrollback + 清屏 + 光标归位：直接喂给 view（不经 shell，故不产生回显）。
// CSI 3J → TerminalCore::sinkSbClear 清历史；CSI 2J 清屏；CSI H 归位。
const QByteArray kClearSeq = QByteArrayLiteral("\x1b[3J\x1b[2J\x1b[H");
} // namespace

WrapPane::WrapPane(QWidget *parent) : QWidget(parent), m_pty(std::make_unique<ConPty>()) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_view = new TerminalView(this);
    m_view->setFocusPolicy(Qt::NoFocus); // 只读：焦点始终留搜索框（REPL 输入行）
    m_layout->addWidget(m_view);

    // 网格尺寸变化 → ConPTY resize（ConPTY 参数是 cols,rows）。view/pty 均随 pane 长存，
    // 这条连接只连一次。
    connect(m_view, &TerminalView::resized, this, [this](int r, int c) {
        m_pty->resize(static_cast<short>(c), static_cast<short>(r));
    });

    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

WrapPane::~WrapPane() { teardownSession(); }

void WrapPane::startSession() {
    if (m_started)
        return;

    const int rows = m_view->gridRows();
    const int cols = m_view->gridCols();
    if (!m_pty->start(shellCommandLine(), static_cast<short>(cols), static_cast<short>(rows)))
        return;

#ifdef Q_OS_WIN
    m_reader = new PtyReader(this);
    m_reader->setReadHandle(m_pty->readHandle());
    // PTY 输出 → 视图（跨线程 Queued）。reader 每次起会话新建，故这些连接放这里。
    connect(m_reader, &PtyReader::bytesRead, m_view, &TerminalView::onBytes);
    connect(m_reader, &PtyReader::eof, this, &WrapPane::onEof);
    m_reader->start();
#endif
    m_started = true;
}

void WrapPane::run(const QString &cmd) {
    const QString trimmed = cmd.trimmed();
    if (trimmed.isEmpty())
        return;

    startSession(); // 幂等；仅首条命令时真正启动常驻会话
    if (!m_started) {
        m_view->onBytes((I18n::t("wrap.failed").arg(trimmed) + "\r\n").toUtf8());
        emit finished();
        return;
    }

    m_view->onBytes(kClearSeq); // 清屏，只显示本条命令的输出
    const QByteArray bytes = trimmed.toUtf8() + "\r";
    m_pty->write(bytes.constData(), bytes.size());
}

void WrapPane::cancel() { teardownSession(); }

bool WrapPane::isRunning() const { return m_started; }

void WrapPane::teardownSession() {
    if (m_reader) {
        m_reader->requestStop();
        m_pty->stop();        // 关句柄，解锁阻塞的 ReadFile
        m_reader->wait(3000); // join
        m_reader->deleteLater();
        m_reader = nullptr;
    } else {
        m_pty->stop();
    }
    m_started = false;
}

void WrapPane::onEof() {
    teardownSession(); // 常驻 shell 退出（如 exit）；保留 view 已渲染内容
    emit finished();
}

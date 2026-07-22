#include "terminalpane.h"
#include "core/theme.h"
#include "pty/conpty.h"
#include "pty/ptyreader.h"
#include "ui/terminalview.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
// 落盘一个 pwsh prompt 钩子脚本：每次刷新提示符时把窗口标题设为当前目录（ProviderPath），
// 让多标签能取到"最后一层目录名"。PowerShell 的 Set-Location 不改进程 cwd，标题是唯一可靠信号。
// 脚本包住已有 prompt（oh-my-posh 等），不破坏其渲染。返回脚本路径，失败返回空。
QString ensureCwdTitleHook() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        QStringLiteral("/shell");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/pwsh-cwd-title.ps1");
    static const char *kScript =
        // 强制 pwsh 输出 ANSI 颜色：Host 渲染在自建 ConPTY 里会判定为不支持 VT 而剥色，
        // 显式设 Ansi 让 Get-ChildItem 等无条件带色（5.1 无 $PSStyle，if 守卫安全跳过）。
        "if ($PSStyle) { $PSStyle.OutputRendering = 'Ansi' }\r\n"
        "# launcher 注入：提示符刷新时把窗口标题设为当前目录（供多标签取最后一层目录名）。\r\n"
        "# 包住已有 prompt（如 oh-my-posh），不破坏其渲染。\r\n"
        "$global:__lnPrevPrompt = $function:prompt\r\n"
        "function global:prompt {\r\n"
        "    try { $Host.UI.RawUI.WindowTitle = "
        "$ExecutionContext.SessionState.Path.CurrentLocation.ProviderPath } catch {}\r\n"
        "    if ($global:__lnPrevPrompt) { & $global:__lnPrevPrompt } else { "
        "\"PS $($ExecutionContext.SessionState.Path.CurrentLocation.Path)> \" }\r\n"
        "}\r\n";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(kScript);
        f.close();
    }
    return QFile::exists(path) ? path : QString();
}

// shell 命令行：pwsh(7) > powershell(5.1) > cmd；pwsh/powershell 注入上面的 cwd→标题钩子。
QString shellCommandLine() {
    QString exe = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
    if (exe.isEmpty())
        exe = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (exe.isEmpty())
        return QStringLiteral("cmd.exe"); // cmd 无 cwd→标题钩子，标签回退 "Terminal N"
    const QString hook = ensureCwdTitleHook();
    if (hook.isEmpty())
        return QString(QStringLiteral("\"%1\"")).arg(exe);
    // 用户 profile（含 oh-my-posh）先于 -File 加载，故钩子能包住其 prompt
    return QString(QStringLiteral("\"%1\" -NoExit -File \"%2\"")).arg(exe, hook);
}
} // namespace

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent), m_pty(std::make_unique<ConPty>()) {
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_view = new TerminalView(this);
    lay->addWidget(m_view);

    // m_view 与 m_pty 都随 pane 生命周期长存，这两条连接**只连一次**（放构造函数）。
    // 若放 startSession 里，重开会话会重复连接 → 每个键写多次（经典重复输入 bug）。
    // 键盘编码 / 查询响应 → 写回 PTY（无会话时 m_pty->write 内部直接返回，无害）
    connect(m_view, &TerminalView::outputToPty, this, [this](const QByteArray &b) {
        m_pty->write(b.constData(), b.size());
    });
    // 网格尺寸变化 → ConPTY resize（注意 ConPTY 参数是 cols,rows）
    connect(m_view, &TerminalView::resized, this, [this](int r, int c) {
        m_pty->resize(static_cast<short>(c), static_cast<short>(r));
    });

    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

TerminalPane::~TerminalPane() { teardown(); }

void TerminalPane::startSession() {
    if (m_started)
        return;

    const int rows = m_view->gridRows();
    const int cols = m_view->gridCols();

    if (!m_pty->start(shellCommandLine(), static_cast<short>(cols),
                      static_cast<short>(rows)))
        return;

#ifdef Q_OS_WIN
    m_reader = new PtyReader(this);
    m_reader->setReadHandle(m_pty->readHandle());

    // PTY 输出 → 视图（跨线程 Queued，自动）。m_reader 每会话新建，故这些连接放这里。
    connect(m_reader, &PtyReader::bytesRead, m_view, &TerminalView::onBytes);
    connect(m_reader, &PtyReader::eof, this, &TerminalPane::onSessionEnded);

    m_reader->start();
#endif

    m_started = true;
    m_view->setFocus();
}

void TerminalPane::runCommand(const QString &cmd) {
    startSession(); // 幂等；只在方法内触碰 m_pty，不新增任何长存信号连接
    if (!m_started || cmd.isEmpty())
        return;
    const QByteArray bytes = cmd.toUtf8() + "\r";
    m_pty->write(bytes.constData(), bytes.size());
}

void TerminalPane::focusTerminal() {
    if (m_view)
        m_view->setFocus();
}

void TerminalPane::onSessionEnded() {
    teardown(); // 允许下次重新起会话
    emit sessionEnded();
}

void TerminalPane::teardown() {
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

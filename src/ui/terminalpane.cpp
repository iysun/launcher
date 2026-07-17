#include "terminalpane.h"
#include "core/theme.h"
#include "pty/conpty.h"
#include "pty/ptyreader.h"
#include "ui/terminalview.h"

#include <QKeyEvent>
#include <QRegion>
#include <QResizeEvent>
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

    // 退出模式键（Ctrl+`）须抢在 TerminalView 把它当普通按键 raw 送 PTY 之前拦截，
    // 故装在 m_view 上；Esc 等其余按键一律放行给终端（vim/htop 的命门）。
    m_view->installEventFilter(this);

    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

TerminalPane::~TerminalPane() { teardown(); }

void TerminalPane::startSession() {
    if (m_started)
        return;

    const int rows = m_view->gridRows();
    const int cols = m_view->gridCols();

    if (!m_pty->start(pickShell(), static_cast<short>(cols), static_cast<short>(rows)))
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

void TerminalPane::setCornerRadius(int r) {
    m_cornerRadius = r;
    applyCornerMask();
}

// TerminalView 是不透明的（逐格填满），会盖掉宿主卡片的圆角。这里用圆角 QRegion
// 遮罩把 pane（含子控件）裁成圆角，与卡片圆角对齐。r==0 时清除遮罩（原生方框）。
void TerminalPane::applyCornerMask() {
    if (m_cornerRadius <= 0 || width() <= 0 || height() <= 0) {
        clearMask();
        return;
    }
    const int  d = m_cornerRadius * 2;
    const QRect r = rect();
    QRegion    reg(r.adjusted(m_cornerRadius, 0, -m_cornerRadius, 0)); // 中间横条
    reg += QRegion(r.adjusted(0, m_cornerRadius, 0, -m_cornerRadius)); // 中间竖条
    reg += QRegion(r.left(), r.top(), d, d, QRegion::Ellipse);                 // 左上
    reg += QRegion(r.right() - d + 1, r.top(), d, d, QRegion::Ellipse);        // 右上
    reg += QRegion(r.left(), r.bottom() - d + 1, d, d, QRegion::Ellipse);      // 左下
    reg += QRegion(r.right() - d + 1, r.bottom() - d + 1, d, d, QRegion::Ellipse); // 右下
    setMask(reg);
}

void TerminalPane::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    applyCornerMask(); // 尺寸变了圆角遮罩要跟着重算，否则裁剪错位
}

bool TerminalPane::eventFilter(QObject *obj, QEvent *e) {
    if (obj == m_view && e->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(e);
        if ((key->modifiers() & Qt::ControlModifier) &&
            key->key() == Qt::Key_QuoteLeft) { // Ctrl+` 退出终端模式
            emit exitRequested();
            return true; // 吞掉，不下送 PTY
        }
    }
    return QWidget::eventFilter(obj, e);
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

#include "wrappane.h"
#include "core/i18n.h"
#include "core/theme.h"

#include <QDir>
#include <QFont>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
QString stripAnsiEscapes(QString text) {
    // CSI: ESC [ … final byte；以及 ESC 丢失时残留的 [33;1m 等
    static const QRegularExpression kCsi(
        QStringLiteral("\x1B\\[[0-9;]*[ -/]*[@-~]"));
    static const QRegularExpression kOrphanCsi(QStringLiteral("\\[[0-9;]*m"));
    text.replace(kCsi, QString());
    text.replace(kOrphanCsi, QString());
    return text;
}

QString shellProgram() {
    if (!QStandardPaths::findExecutable(QStringLiteral("pwsh")).isEmpty())
        return QStringLiteral("pwsh");
    if (!QStandardPaths::findExecutable(QStringLiteral("powershell")).isEmpty())
        return QStringLiteral("powershell");
    return QStringLiteral("cmd");
}

QStringList shellArguments(const QString &program, const QString &cmd) {
    if (program == QStringLiteral("cmd"))
        return {QStringLiteral("/c"), cmd};
    // pwsh/powershell：PlainText 避免目录列表等自带 ANSI 着色
    const QString plain =
        QStringLiteral("$PSStyle.OutputRendering='PlainText'; ") + cmd;
    return {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
            QStringLiteral("-Command"), plain};
}
} // namespace

WrapPane::WrapPane(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_output->setFocusPolicy(Qt::StrongFocus);
    m_output->setFont(QFont(QStringLiteral("Consolas")));
    m_output->setStyleSheet(QString(R"(
        QPlainTextEdit {
            background: transparent;
            color: %1;
            border: none;
            border-top: 1px solid %2;
            font-size: 13px;
            padding: 8px 12px;
        }
    )")
                                 .arg(Theme::c("text"), Theme::c("surface")));

    root->addWidget(m_output);
    setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::c("bg")));
}

void WrapPane::run(const QString &cmd) {
    cancel();
    m_output->clear();
    m_output->setPlainText(I18n::t("wrap.running"));
    startProcess(cmd.trimmed());
}

void WrapPane::cancel() {
    if (!m_proc)
        return;
    m_proc->disconnect(this);
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
    m_proc->deleteLater();
    m_proc = nullptr;
}

bool WrapPane::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void WrapPane::startProcess(const QString &cmd) {
    const QString program = shellProgram();
    m_proc                = new QProcess(this);
    m_proc->setProgram(program);
    m_proc->setArguments(shellArguments(program, cmd));
    m_proc->setWorkingDirectory(QDir::homePath());
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("TERM"), QStringLiteral("dumb"));
    m_proc->setProcessEnvironment(env);

    connect(m_proc, &QProcess::readyRead, this, &WrapPane::onReadyRead);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &WrapPane::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred, this, &WrapPane::onProcessError);

    m_output->clear();
    m_proc->start();
}

void WrapPane::onReadyRead() {
    if (!m_proc)
        return;
    appendOutput(m_proc->readAll());
}

void WrapPane::appendOutput(const QByteArray &bytes) {
    if (bytes.isEmpty())
        return;
    if (m_output->toPlainText() == I18n::t("wrap.running"))
        m_output->clear();
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(stripAnsiEscapes(QString::fromUtf8(bytes)));
    m_output->moveCursor(QTextCursor::End);
}

void WrapPane::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    if (m_proc)
        appendOutput(m_proc->readAll());
    const QString cleaned = stripAnsiEscapes(m_output->toPlainText());
    if (cleaned != m_output->toPlainText())
        m_output->setPlainText(cleaned);
    if (m_output->toPlainText().trimmed().isEmpty())
        m_output->setPlainText(I18n::t("wrap.empty"));
    else if (exitCode != 0) {
        m_output->appendPlainText(
            QStringLiteral("\n[%1 %2]").arg(I18n::t("wrap.exitCode")).arg(exitCode));
    }
    m_proc = nullptr;
    emit finished();
}

void WrapPane::onProcessError() {
    if (!m_proc)
        return;
    m_output->setPlainText(
        I18n::t("wrap.failed").arg(m_proc->errorString()));
    m_proc = nullptr;
    emit finished();
}

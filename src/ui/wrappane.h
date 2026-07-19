#pragma once
#include <QProcess>
#include <QWidget>

class QPlainTextEdit;

// launcher 形态内的一次性命令输出：QProcess 跑单条命令，只读文本区展示 stdout+stderr。
class WrapPane : public QWidget {
    Q_OBJECT
public:
    explicit WrapPane(QWidget *parent = nullptr);

    void run(const QString &cmd);
    void cancel();
    bool isRunning() const;

signals:
    void finished();

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError();

private:
    void appendOutput(const QByteArray &bytes);
    void startProcess(const QString &cmd);

    QPlainTextEdit *m_output = nullptr;
    QProcess       *m_proc   = nullptr;
};

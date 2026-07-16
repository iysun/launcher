#pragma once
#include <QByteArray>
#include <QThread>
#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// worker 线程：阻塞读 ConPTY 输出管道，每块数据经 QueuedConnection 发给 GUI 线程。
// 本线程不碰任何 Qt 对象与 TerminalCore，只搬运裸字节。
class PtyReader : public QThread {
    Q_OBJECT
public:
    explicit PtyReader(QObject *parent = nullptr) : QThread(parent) {}

#ifdef Q_OS_WIN
    void setReadHandle(HANDLE h) { m_read = h; }
#endif
    void requestStop() { m_stop.store(true); }

signals:
    void bytesRead(const QByteArray &chunk); // → TerminalView::onBytes（Queued）
    void eof();                              // 管道关闭 / 子进程退出

protected:
    void run() override;

private:
#ifdef Q_OS_WIN
    HANDLE m_read = INVALID_HANDLE_VALUE;
#endif
    std::atomic<bool> m_stop{false};
};

#pragma once
#include <QWidget>
#include <memory>

class TerminalView;
class ConPty;
class PtyReader;

// 独立顶层终端窗口：内嵌 TerminalView，持有 ConPty + PtyReader，接线信号槽。
// 由 /terminal 命令复用同一单例。MVP 采用原生窗口边框（可缩放）。
class TerminalWindow : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWindow(QWidget *parent = nullptr);
    ~TerminalWindow() override;

    void startSession(); // 懒启动 shell，幂等

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onSessionEnded();

private:
    void teardown();

    TerminalView           *m_view = nullptr;
    std::unique_ptr<ConPty> m_pty;
    PtyReader              *m_reader  = nullptr;
    bool                    m_started = false;
};

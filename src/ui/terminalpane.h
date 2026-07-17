#pragma once
#include <QWidget>
#include <memory>

class TerminalView;
class ConPty;
class PtyReader;

// 单个终端会话面板：自包含 TerminalView + ConPty + PtyReader + 会话生命周期接线。
// 纯会话 widget，不自管任何快捷键/圆角——终端模式的快捷键拦截与圆角遮罩统一由外层
// TerminalTabs 处理。底层 ConPty 多实例无限制，多标签即多 new 一个本类。
class TerminalPane : public QWidget {
    Q_OBJECT
public:
    explicit TerminalPane(QWidget *parent = nullptr);
    ~TerminalPane() override;

    void startSession();                 // 懒启动 shell，幂等
    void runCommand(const QString &cmd); // 未启动会话则先 startSession；空命令 = 只保证会话存在
    bool isRunning() const { return m_started; }
    void focusTerminal();                // 把键盘焦点交给内嵌 TerminalView
    void teardown();                     // 停 reader / stop pty / join，复位以便重开会话

    // 供 TerminalTabs 在其上装 event filter（拦截快捷键）、连 core 的 titleChanged。
    TerminalView *view() const { return m_view; }

signals:
    void sessionEnded(); // shell 退出（会话已 teardown），容器据此关掉该标签

private slots:
    void onSessionEnded();

private:
    TerminalView           *m_view = nullptr;
    std::unique_ptr<ConPty> m_pty;
    PtyReader              *m_reader  = nullptr;
    bool                    m_started = false;
};

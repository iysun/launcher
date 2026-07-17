#pragma once
#include <QWidget>
#include <memory>

class TerminalView;
class ConPty;
class PtyReader;

// 可嵌入的终端面板：自包含 TerminalView + ConPty + PtyReader + 会话生命周期接线。
// 无顶层窗口语义（不设标题/尺寸/closeEvent），既可被 MainWindow 内联进卡片做"终端模式"，
// 也可被 TerminalWindow 作为顶层窗口内容复用。底层 ConPty 多实例无限制，两处各持独立会话。
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
    // 圆角遮罩：>0 时按半径把 pane（含内嵌不透明 TerminalView）裁成圆角，随尺寸自动重算。
    // 供 MainWindow 内联模式匹配卡片圆角用；独立 TerminalWindow 保持 0（原生方框）。
    void setCornerRadius(int r);

signals:
    void sessionEnded();  // shell 退出（会话已 teardown）
    void exitRequested(); // 用户在终端内按下退出模式键（Ctrl+`），宿主据此切回搜索

protected:
    // 装在内嵌 TerminalView 上：抢在 TerminalView 把按键 raw 送 PTY 之前拦截退出键。
    bool eventFilter(QObject *obj, QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override; // 尺寸变化时按 m_cornerRadius 重算圆角遮罩

private slots:
    void onSessionEnded();

private:
    void applyCornerMask();

    int                     m_cornerRadius = 0;
    TerminalView           *m_view = nullptr;
    std::unique_ptr<ConPty> m_pty;
    PtyReader              *m_reader  = nullptr;
    bool                    m_started = false;
};

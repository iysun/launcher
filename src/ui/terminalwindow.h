#pragma once
#include <QWidget>

class TerminalPane;

// 独立顶层终端窗口：内嵌一个可复用 TerminalPane，提供原生可缩放窗口边框。
// 由 /termwin 命令复用同一单例；与 MainWindow 的内联终端模式各持独立会话。
class TerminalWindow : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWindow(QWidget *parent = nullptr);

    void startSession(); // 懒启动 shell，幂等（转发给 pane）
    void runCommand(const QString &cmd); // 在当前会话执行命令（转发给 pane）

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    TerminalPane *m_pane = nullptr;
};

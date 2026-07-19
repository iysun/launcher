#pragma once
#include <QWidget>

class TerminalPane;
class QTabBar;
class QStackedWidget;
class QHBoxLayout;

// 终端多标签容器：QTabBar + QStackedWidget，每个标签一个 TerminalPane（独立会话）。
// 宿主为 MainWindow 内联终端模式。终端模式下按键 raw 直送 shell，故所有管理快捷键
// （Ctrl+` 退出、Ctrl+Shift+T/W、Ctrl+Tab、Ctrl+Shift+Tab、Alt+1..9）都在本容器于
// event filter 抢在 TerminalView 消费之前拦截；其余按键（含 Esc）放行给终端。
class TerminalTabs : public QWidget {
    Q_OBJECT
public:
    explicit TerminalTabs(QWidget *parent = nullptr);

    void startSession();                 // 无标签则建一个并起会话；否则确保当前会话在跑
    void openNewTab();                   // 强制新建标签并切换过去（:: / Ctrl+Enter 持久终端）
    void runCommand(const QString &cmd); // 在当前标签执行（无标签先建一个）
    void focusTerminal();                // 聚焦当前 pane
    void teardown();                     // teardown 所有 pane（宿主关闭/退出时）
    void setCornerRadius(int r);         // 圆角遮罩（贴合卡片圆角）
    int  count() const;

signals:
    void exitRequested(); // Ctrl+` 冒泡：宿主退出终端模式 / 关窗
    void lastTabClosed(); // 关掉最后一个标签：宿主退出终端模式 / 关窗

protected:
    bool eventFilter(QObject *obj, QEvent *e) override; // 拦截管理快捷键
    void resizeEvent(QResizeEvent *e) override;         // 重算圆角遮罩

private slots:
    void closeTab(int index);

private:
    TerminalPane *newTab();            // 建标签 + 起会话 + 设当前，返回新 pane
    TerminalPane *currentPane() const;
    void          nextTab();
    void          prevTab();
    void          jumpTab(int index);
    void          applyCornerMask();
    void          updateStripVisibility(); // 单标签时隐藏标签栏，保持干净单会话观感

    QWidget        *m_strip    = nullptr;
    QHBoxLayout    *m_stripLay = nullptr;
    QTabBar        *m_bar   = nullptr;
    QStackedWidget *m_stack = nullptr;
    int             m_cornerRadius = 0;
    int             m_seq = 0;             // "Terminal N" 回退命名计数（只增，标签名稳定）
};

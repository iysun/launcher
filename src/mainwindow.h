#pragma once
#include "plugin/iplugin.h"
#include <QWidget>

class AppSettings;
class QFrame;
class QHotkey;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSystemTrayIcon;
class QTimer;
class QVBoxLayout;
class ResultDelegate;
class TerminalTabs;
class UsageStore;
class WrapPane;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(AppSettings *settings, QWidget *parent = nullptr);
    void                    addPlugin(IPlugin *plugin);
    const QList<IPlugin *> &plugins() const { return m_plugins; }

    // 内联终端模式（融合入口）：把卡片切换为内嵌交互终端，键盘 raw 直送 shell。
    // cmd 非空则进入后立即执行该命令。newTab 为真则强制新开标签。会话后台留存，再次进入即续。
    void enterTerminal(const QString &cmd = QString(), bool newTab = false);
    void exitTerminal(); // 切回搜索模式（不 teardown 会话）

    // Wrap 模式：launcher 卡片内一次性展示命令输出（QProcess，非 PTY）。
    void runWrap(const QString &cmd);
    void exitWrap();

protected:
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void onTextChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);
    void runQuery(); // 防抖到期后真正执行查询（异步化的接缝点）
    void toggle();

private:
    enum class Mode { Launch, Terminal, Wrap }; // 搜索 / 内联终端 / 一次性输出

    void  setupUi();
    void  showResults(const QList<ResultItem> &items);
    void  mergeAndShow(); // 合并 base+async → 排序 → 截断 → 主线程装饰 → 展示
    void  onAsyncResults(IPlugin                 *p,
                         const QList<ResultItem> &r); // 异步结果到达（已过代次校验）
    void  centerOnScreen();
    void  notifyHotkeyStatus();  // 全局热键注册失败时用托盘气泡提示（否则用户只见"唤不起来"）
    void  flushPendingQuery(); // 回车前冲刷防抖，确保作用于最新关键词的结果
    void  activate(QListWidgetItem *item, bool alt); // alt=Ctrl+Enter 触发次级动作
    bool  handleEmacsKey(QKeyEvent *key);            // Emacs 风格键位，消费则返回 true
    void  moveSelection(int delta);                  // 移动列表选中项（焦点不离开搜索框）

    QFrame          *m_card       = nullptr; // 圆角卡片容器（终端模式改其固定宽）
    QVBoxLayout     *m_cardLayout = nullptr; // 卡片内布局（终端 pane 懒挂于此）
    TerminalTabs    *m_term       = nullptr; // 内联终端多标签（懒建，跨模式切换后台留存）
    WrapPane        *m_wrap       = nullptr; // 一次性命令输出（懒建，Wrap 模式）
    Mode             m_mode       = Mode::Launch;

    QLineEdit       *m_search;
    QListWidget     *m_list;
    QHotkey         *m_hotkey;
    QSystemTrayIcon *m_tray;
    QTimer          *m_queryTimer;
    ResultDelegate  *m_delegate;
    UsageStore      *m_usage;
    AppSettings     *m_settings;
    QString          m_pendingKeyword; // 防抖期间暂存的查询词
    QList<IPlugin *> m_plugins;

    int               m_queryGen = 0; // 查询代次，异步回调据此丢弃过期结果
    QList<ResultItem> m_baseResults;  // 当前代次的同步插件结果
    QList<ResultItem> m_asyncResults; // 当前代次累积的异步结果
};

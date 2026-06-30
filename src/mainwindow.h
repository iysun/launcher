#pragma once
#include "plugin/iplugin.h"
#include <QWidget>

class AppSettings;
class QHotkey;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
class ResultDelegate;
class UsageStore;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(AppSettings *settings, QWidget *parent = nullptr);
    void addPlugin(IPlugin *plugin);
    const QList<IPlugin *>& plugins() const { return m_plugins; }

protected:
    void changeEvent(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void onTextChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);
    void runQuery();  // 防抖到期后真正执行查询（异步化的接缝点）
    void toggle();

private:
    void setupUi();
    void showResults(const QList<ResultItem> &items);
    void mergeAndShow();  // 合并 base+async → 排序 → 截断 → 主线程装饰 → 展示
    void onAsyncResults(IPlugin *p, const QList<ResultItem> &r);  // 异步结果到达（已过代次校验）
    void centerOnScreen();
    void flushPendingQuery();  // 回车前冲刷防抖，确保作用于最新关键词的结果
    void activate(QListWidgetItem *item, bool alt);  // alt=Ctrl+Enter 触发次级动作
    bool handleEmacsKey(QKeyEvent *key);  // Emacs 风格键位，消费则返回 true
    void moveSelection(int delta);        // 移动列表选中项（焦点不离开搜索框）

    QLineEdit      *m_search;
    QListWidget    *m_list;
    QHotkey        *m_hotkey;
    QTimer         *m_queryTimer;
    ResultDelegate *m_delegate;
    UsageStore     *m_usage;
    AppSettings    *m_settings;
    QString          m_pendingKeyword;  // 防抖期间暂存的查询词
    QList<IPlugin *> m_plugins;

    int               m_queryGen = 0;   // 查询代次，异步回调据此丢弃过期结果
    QList<ResultItem> m_baseResults;    // 当前代次的同步插件结果
    QList<ResultItem> m_asyncResults;   // 当前代次累积的异步结果
};

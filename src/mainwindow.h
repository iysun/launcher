#pragma once
#include "plugin/iplugin.h"
#include <QWidget>

class QHotkey;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
class ResultDelegate;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void addPlugin(IPlugin *plugin);

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
    void centerOnScreen();
    void flushPendingQuery();  // 回车前冲刷防抖，确保作用于最新关键词的结果

    QLineEdit      *m_search;
    QListWidget    *m_list;
    QHotkey        *m_hotkey;
    QTimer         *m_queryTimer;
    ResultDelegate *m_delegate;
    QString          m_pendingKeyword;  // 防抖期间暂存的查询词
    QList<IPlugin *> m_plugins;
};

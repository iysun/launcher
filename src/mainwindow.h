#pragma once
#include "plugin/iplugin.h"
#include <QWidget>

class QHotkey;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
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
    void toggle();

private:
    void setupUi();
    void showResults(const QList<ResultItem> &items);
    void centerOnScreen();

    QLineEdit      *m_search;
    QListWidget    *m_list;
    QHotkey        *m_hotkey;
    ResultDelegate *m_delegate;
    QList<IPlugin *> m_plugins;
};

#pragma once
#include "plugin/iplugin.h"
#include <QObject>

class QFileSystemWatcher;
class QTimer;

// 搜索已安装应用
// Windows: 扫描 Start Menu 的 .lnk 文件
// Linux:   扫描 /usr/share/applications 的 .desktop 文件
// 监听扫描目录，装/卸应用后去抖重扫，免重启刷新。
class AppPlugin : public QObject, public IPlugin {
    Q_OBJECT
public:
    AppPlugin();
    QString           name()  const override { return "AppLauncher"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

private:
    void loadApps();                            // 全量重扫 m_apps 并刷新监听目录
    void updateWatch(const QStringList &dirs);  // 用最新目录集替换 watcher 监听

    QFileSystemWatcher *m_watcher;
    QTimer             *m_reloadTimer;  // 合并目录变更的突发事件，去抖后重扫
    QList<ResultItem>   m_apps;         // 预加载，查询时只过滤
};

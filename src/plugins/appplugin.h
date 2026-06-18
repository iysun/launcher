#pragma once
#include "plugin/iplugin.h"

// 搜索已安装应用
// Windows: 扫描 Start Menu 的 .lnk 文件
// Linux:   扫描 /usr/share/applications 的 .desktop 文件
class AppPlugin : public IPlugin {
public:
    AppPlugin();
    QString           name()  const override { return "AppLauncher"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

private:
    void              loadApps();
    QList<ResultItem> m_apps;  // 启动时预加载，查询时只过滤
};

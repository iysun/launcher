#pragma once
#include "plugin/iplugin.h"
#include <QString>

class AppSettings;

struct WebEngine {
    QString id;
    QString name;
    QString urlTemplate; // %1 为 URL 编码后的关键词
};

// 网页搜索，"?" 前缀触发。
// 结果顺序由 AppSettings::webEngineOrder() 决定，用大分差保证用户排序优先于 frecency。
class WebPlugin : public IPlugin {
public:
    explicit WebPlugin(AppSettings *settings = nullptr);

    QString           name() const override { return "Web"; }
    QString           triggerPrefix() const override { return "?"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

    // 供设置页读取引擎元数据（id / 显示名称）
    static QList<WebEngine> allEngines();

private:
    AppSettings *m_settings;
};

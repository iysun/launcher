#pragma once
#include "core/webengine.h"
#include "plugin/iplugin.h"
#include <optional>
#include <QString>

class AppSettings;

// 网页搜索，"?" 前缀触发。
// 结果顺序由 AppSettings::webEngineOrder() 决定，用大分差保证用户排序优先于 frecency。
class WebPlugin : public IPlugin {
public:
    explicit WebPlugin(AppSettings *settings = nullptr);

    QString           name() const override { return "Web"; }
    QString           triggerPrefix() const override { return "?"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

    // 供设置页读取引擎元数据（id / 显示名称）；settings 非空时会带上用户自定义引擎
    static QList<WebEngine> allEngines(AppSettings *settings = nullptr);

private:
    // 按值返回：customWebEngines() 每次调用都是新的临时 QList，返回指针会悬空
    std::optional<WebEngine> findEngine(const QString &id) const;

    AppSettings *m_settings;
};

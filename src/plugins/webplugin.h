#pragma once
#include "plugin/iplugin.h"

// 网页搜索，"?" 前缀触发。
// 每个搜索引擎作为一条结果，回车用默认浏览器打开；
// action 键按引擎 id 计入 frecency，用多哪个就自动排前面。
class WebPlugin : public IPlugin {
public:
    QString           name()         const override { return "Web"; }
    QString           triggerPrefix() const override { return "?"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;
};

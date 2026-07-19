#pragma once
#include "plugin/iplugin.h"
#include <functional>

// 持久终端命令，"::" 前缀触发：`:: vim` 回车 → 新开标签页并进入内联终端执行。
// runner 由 main.cpp 装配（enterTerminal + newTab），插件不依赖 ui 头。
class RunPersistentPlugin : public IPlugin {
public:
    using Runner = std::function<void(const QString &cmd)>;
    explicit RunPersistentPlugin(Runner runner) : m_runner(std::move(runner)) {}

    QString           name() const override { return "RunPersistent"; }
    QString           triggerPrefix() const override { return "::"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

private:
    Runner m_runner;
};

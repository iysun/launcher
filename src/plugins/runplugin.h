#pragma once
#include "plugin/iplugin.h"
#include <functional>

// 命令执行，":" 前缀触发：`: ipconfig` 回车 → 内置终端窗口弹出并执行该命令。
// 与 CommandPlugin 同款解耦：runner 由 main.cpp 装配（打开终端 + 写入命令），
// 插件不依赖任何 ui 头。frecency 由 MainWindow 按 action 键自动记录。
class RunPlugin : public IPlugin {
public:
    using Runner = std::function<void(const QString &cmd)>;
    explicit RunPlugin(Runner runner) : m_runner(std::move(runner)) {}

    QString           name() const override { return "Run"; }
    QString           triggerPrefix() const override { return ":"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

private:
    Runner m_runner;
};

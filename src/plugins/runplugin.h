#pragma once
#include "plugin/iplugin.h"
#include <functional>

// 命令执行，":" 前缀触发：`: ls` Enter → launcher 内一次性展示输出；
// Ctrl+Enter → 新开标签进内联终端。runner 由 main.cpp 装配，插件不依赖 ui 头。
class RunPlugin : public IPlugin {
public:
    using Runner = std::function<void(const QString &cmd)>;
    explicit RunPlugin(Runner wrapRunner, Runner terminalRunner);

    QString           name() const override { return "Run"; }
    QString           triggerPrefix() const override { return ":"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;
    void              executeAlt(const ResultItem &item) override;

private:
    Runner m_wrapRunner;
    Runner m_terminalRunner;
};

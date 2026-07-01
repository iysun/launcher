#pragma once
#include "plugin/iplugin.h"
#include <functional>
#include <QIcon>

class HelpDialog;

// launcher 自身命令，"/" 前缀触发（退出 / 重载应用 / 打开数据目录 / 重启…）。
// 设计为通用命令注册表：动作用 std::function 表达，由 main.cpp 按需装配，
// 插件本身不与具体能力耦合。
class CommandPlugin : public IPlugin {
public:
    using Action = std::function<void()>;

    CommandPlugin(); // 自注册内置 help 命令

    QString           name() const override { return "Commands"; }
    QString           triggerPrefix() const override { return "/"; }
    QList<ResultItem> query(const QString &keyword) override;
    void              execute(const ResultItem &item) override;

    // id：敲入的命令词（如 quit），兼作显示主行 "/id" 与 frecency 稳定键；
    // desc：简要描述，作副行，与 id 一并参与模糊匹配；run：执行动作。
    void addCommand(const QString &id, const QString &desc, Action run,
                    const QIcon &icon = {});

private:
    void showHelp(); // 弹出帮助：用法 + 当前全部命令

    struct Command {
        QString id, desc;
        QIcon   icon;
        Action  run;
    };
    QList<Command> m_cmds;
    HelpDialog    *m_helpDialog = nullptr; // 懒初始化，持有所有权
};

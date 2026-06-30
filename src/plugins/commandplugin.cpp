#include "commandplugin.h"
#include "core/matcher.h"
#include "ui/helpdialog.h"

CommandPlugin::CommandPlugin() {
    // help 内置自注册：showHelp 在执行时读 m_cmds，故能列出 main.cpp 后续装配的全部命令
    addCommand("help", "显示帮助", [this] { showHelp(); });
}

void CommandPlugin::addCommand(const QString &id, const QString &desc,
                               Action run, const QIcon &icon) {
    m_cmds.append({id, desc, icon, std::move(run)});
}

QList<ResultItem> CommandPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();

    // 只筛选 + 打分；排序与截断由 MainWindow 跨插件统一处理
    QList<ResultItem> results;
    for (const auto &c : m_cmds) {
        int s;
        if (kw.isEmpty()) {
            s = 0;  // 裸 "/"：列出全部命令（命令面板），同分时由 frecency 把常用项上浮
        } else {
            // 同时按 id 与描述匹配，取较高分：/quit 与 /退出 都能命中
            s = qMax(Matcher::score(c.id, kw), Matcher::score(c.desc, kw));
            if (s < 0) continue;
        }
        ResultItem item;
        item.title    = "/" + c.id;  // PowerToys 风格：主行展示要敲的命令
        item.subtitle = c.desc;      // 副行为简要描述
        item.action   = c.id;        // 稳定键：execute 查表 + frecency 计数
        item.icon     = c.icon;
        item.score    = s;
        results.append(item);
    }
    return results;
}

void CommandPlugin::execute(const ResultItem &item) {
    for (const auto &c : m_cmds) {
        if (c.id == item.action) {
            if (c.run) c.run();
            return;
        }
    }
}

void CommandPlugin::showHelp() {
    if (!m_helpDialog)
        m_helpDialog = new HelpDialog();

    QList<QPair<QString, QString>> cmds;
    for (const auto &c : m_cmds)
        cmds.append({c.id, c.desc});
    m_helpDialog->setCommands(cmds);

    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

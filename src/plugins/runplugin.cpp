#include "runplugin.h"
#include "core/i18n.h"

RunPlugin::RunPlugin(Runner wrapRunner, Runner terminalRunner)
    : m_wrapRunner(std::move(wrapRunner)), m_terminalRunner(std::move(terminalRunner)) {}

QList<ResultItem> RunPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();
    if (kw.startsWith(QLatin1Char(':')))
        return {}; // ":: cmd" 时 sub 为 ": cmd"，交给 RunPersistentPlugin

    ResultItem item;
    if (kw.isEmpty()) {
        item.title    = I18n::t("run.hint");
        item.subtitle = I18n::t("run.hintSub");
        item.action   = "run:";
    } else {
        item.title    = I18n::t("run.execute").arg(kw);
        item.subtitle = I18n::t("run.executeSub");
        item.action   = "run:" + kw;
        item.score    = 1000;
    }
    return {item};
}

void RunPlugin::execute(const ResultItem &item) {
    const QString cmd = item.action.mid(4);
    if (cmd.isEmpty() || !m_wrapRunner)
        return;
    m_wrapRunner(cmd);
}

void RunPlugin::executeAlt(const ResultItem &item) {
    const QString cmd = item.action.mid(4);
    if (cmd.isEmpty() || !m_terminalRunner)
        return;
    m_terminalRunner(cmd);
}

#include "runpersistentplugin.h"
#include "core/i18n.h"

QList<ResultItem> RunPersistentPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();

    ResultItem item;
    if (kw.isEmpty()) {
        item.title    = I18n::t("runp.hint");
        item.subtitle = I18n::t("runp.hintSub");
        item.action   = "runp:";
    } else {
        item.title    = I18n::t("runp.execute").arg(kw);
        item.subtitle = I18n::t("runp.executeSub");
        item.action   = "runp:" + kw;
        item.score    = 1000;
    }
    return {item};
}

void RunPersistentPlugin::execute(const ResultItem &item) {
    if (!m_runner)
        return;
    m_runner(item.action.mid(5)); // 去掉 "runp:" 前缀；空串 = 只开新标签
}

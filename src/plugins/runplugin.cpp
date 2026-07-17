#include "runplugin.h"
#include "core/i18n.h"

QList<ResultItem> RunPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();

    ResultItem item;
    if (kw.isEmpty()) {
        // 裸 ":"：提示项，执行 = 只打开终端窗口
        item.title    = I18n::t("run.hint");
        item.subtitle = I18n::t("run.hintSub");
        item.action   = "run:";
    } else {
        item.title    = I18n::t("run.execute").arg(kw);
        item.subtitle = I18n::t("run.executeSub");
        item.action   = "run:" + kw; // 兼 frecency 键：常用命令自动上浮（同档内）
        item.score    = 1000;
    }
    return {item};
}

void RunPlugin::execute(const ResultItem &item) {
    if (!m_runner)
        return;
    m_runner(item.action.mid(4)); // 去掉 "run:" 前缀；空串 = 只开终端
}

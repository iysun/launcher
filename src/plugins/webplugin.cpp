#include "webplugin.h"
#include <QDesktopServices>
#include <QUrl>

struct Engine {
    const char *id;
    const char *name;
    const char *urlTemplate;  // %1 替换为 URL 编码后的关键词
};

static constexpr Engine kEngines[] = {
    {"google", "Google", "https://www.google.com/search?q=%1"},
    {"bing",   "Bing",   "https://www.bing.com/search?q=%1"},
    {"baidu",  "Baidu",  "https://www.baidu.com/s?wd=%1"},
    {"github", "GitHub", "https://github.com/search?q=%1"},
};
static constexpr int kEngineCount = sizeof(kEngines) / sizeof(kEngines[0]);

QList<ResultItem> WebPlugin::query(const QString &keyword) {
    if (keyword.trimmed().isEmpty())
        return {};

    const QString kw = keyword.trimmed();
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(kw));

    QList<ResultItem> results;
    for (int i = 0; i < kEngineCount; ++i) {
        const Engine &e = kEngines[i];
        ResultItem item;
        item.title    = QString("在 %1 中搜索").arg(e.name);
        item.subtitle = kw;
        item.action   = QString("web:%1").arg(e.id);  // frecency 键：按引擎聚合
        item.score    = kEngineCount - i;              // 默认顺序；frecency 会覆盖
        // 将完整 URL 存入 icon 字段之外的途径：借 subtitle 已用，用自定义约定存 URL。
        // 实际 URL 在 execute 时重建，无需存储。
        results.append(item);
    }
    return results;
}

void WebPlugin::execute(const ResultItem &item) {
    // 从 action 反推引擎 id，再从搜索框还原关键词
    // subtitle 就是关键词（query 里填的）
    const QString engineId = item.action.mid(4);  // 去掉 "web:" 前缀
    const QString kw       = item.subtitle;
    const QString encoded  = QString::fromUtf8(QUrl::toPercentEncoding(kw));

    for (int i = 0; i < kEngineCount; ++i) {
        if (engineId == kEngines[i].id) {
            QDesktopServices::openUrl(QUrl(QString(kEngines[i].urlTemplate).arg(encoded)));
            return;
        }
    }
}

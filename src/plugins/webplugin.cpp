#include "webplugin.h"
#include "core/appsettings.h"
#include <QDesktopServices>
#include <QUrl>

static const QList<WebEngine> kAllEngines = {
    {"google", "Google", "https://www.google.com/search?q=%1"},
    {"bing", "Bing", "https://www.bing.com/search?q=%1"},
    {"baidu", "Baidu", "https://www.baidu.com/s?wd=%1"},
    {"github", "GitHub", "https://github.com/search?q=%1"},
};

WebPlugin::WebPlugin(AppSettings *settings) : m_settings(settings) {}

QList<WebEngine> WebPlugin::allEngines(AppSettings *settings) {
    QList<WebEngine> result = kAllEngines;
    if (settings) result += settings->customWebEngines();
    return result;
}

std::optional<WebEngine> WebPlugin::findEngine(const QString &id) const {
    for (const WebEngine &e : kAllEngines)
        if (e.id == id) return e;
    if (m_settings) {
        for (const WebEngine &e : m_settings->customWebEngines())
            if (e.id == id) return e;
    }
    return std::nullopt;
}

QList<ResultItem> WebPlugin::query(const QString &keyword) {
    if (keyword.trimmed().isEmpty()) return {};

    const QString kw = keyword.trimmed();

    // 用户设定的顺序；无设置时回退到默认顺序
    QStringList order = m_settings ? m_settings->webEngineOrder() : QStringList{};
    if (order.isEmpty())
        for (const WebEngine &e : kAllEngines)
            order.append(e.id);

    // 用大分差（步长 100）确保用户手动排序不被 frecency 最大值（60）覆盖
    QList<ResultItem> results;
    int               score = static_cast<int>(order.size()) * 100;
    for (const QString &id : order) {
        const std::optional<WebEngine> e = findEngine(id);
        if (!e) continue;
        ResultItem item;
        item.title    = QString("在 %1 中搜索").arg(e->name);
        item.subtitle = kw;
        item.action   = "web:" + id;
        item.score    = score;
        score -= 100;
        results.append(item);
    }
    return results;
}

void WebPlugin::execute(const ResultItem &item) {
    const QString                  id      = item.action.mid(4); // 去掉 "web:" 前缀
    const QString                  encoded = QString::fromUtf8(QUrl::toPercentEncoding(item.subtitle));
    const std::optional<WebEngine> e       = findEngine(id);
    if (e) QDesktopServices::openUrl(QUrl(e->urlTemplate.arg(encoded)));
}

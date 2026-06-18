#include "appplugin.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

AppPlugin::AppPlugin() {
    loadApps();
}

QList<ResultItem> AppPlugin::query(const QString &keyword) {
    QList<ResultItem> results;
    for (const auto &app : m_apps) {
        if (app.title.contains(keyword, Qt::CaseInsensitive))
            results.append(app);
        if (results.size() >= 8) break;
    }
    return results;
}

void AppPlugin::execute(const ResultItem &item) {
#ifdef Q_OS_WIN
    ShellExecuteW(nullptr, L"open",
                  reinterpret_cast<LPCWSTR>(item.action.utf16()),
                  nullptr, nullptr, SW_SHOW);
#else
    // action 存的是 Exec 字段，去掉 %u %f 等占位符
    QString cmd = item.action;
    cmd.remove(QRegularExpression("%[uUfFdDnNickvm]"));
    QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
#endif
}

// ── 私有：预加载 ──────────────────────────────────────────────

#ifdef Q_OS_WIN
static void scanDir(const QString &path, QList<ResultItem> &out) {
    QDir dir(path);
    for (const QFileInfo &fi : dir.entryInfoList({"*.lnk"}, QDir::Files)) {
        ResultItem item;
        item.title  = fi.completeBaseName();
        item.action = fi.absoluteFilePath();
        item.icon   = QIcon::fromTheme("application-x-executable");
        out.append(item);
    }
    for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        scanDir(path + "/" + sub, out);
}

void AppPlugin::loadApps() {
    const QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
        "C:/ProgramData/Microsoft/Windows/Start Menu/Programs"
    };
    for (const QString &root : roots)
        scanDir(root, m_apps);
}

#else
static ResultItem parseDesktop(const QString &path) {
    ResultItem item;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return item;

    bool inEntry = false;
    for (QTextStream ts(&f); !ts.atEnd();) {
        const QString line = ts.readLine().trimmed();
        if (line == "[Desktop Entry]") { inEntry = true; continue; }
        if (line.startsWith('['))      { inEntry = false; continue; }
        if (!inEntry) continue;

        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();

        if      (key == "Name" && item.title.isEmpty())  item.title  = val;
        else if (key == "Exec" && item.action.isEmpty()) item.action = val;
        else if (key == "Icon")
            item.icon = QIcon::fromTheme(val, QIcon::fromTheme("application-x-executable"));
        else if (key == "NoDisplay" && val == "true")    { item.title.clear(); break; }
    }
    return item;
}

void AppPlugin::loadApps() {
    const QStringList dirs = {
        "/usr/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };
    for (const QString &d : dirs) {
        for (const QFileInfo &fi : QDir(d).entryInfoList({"*.desktop"}, QDir::Files)) {
            ResultItem item = parseDesktop(fi.absoluteFilePath());
            if (!item.title.isEmpty() && !item.action.isEmpty())
                m_apps.append(item);
        }
    }
}
#endif

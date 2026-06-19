#include "appplugin.h"
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <QFileIconProvider>
#  include <objbase.h>
#  include <shlobj.h>
#  include <windows.h>
#endif

AppPlugin::AppPlugin() {
    loadApps();
}

// 匹配打分：完全相等 > 前缀 > 词首 > 普通子串；同档优先短标题。无匹配返回 -1
static int matchScore(const QString &title, const QString &kw) {
    const int idx = title.indexOf(kw, 0, Qt::CaseInsensitive);
    if (idx < 0) return -1;
    if (title.compare(kw, Qt::CaseInsensitive) == 0) return 1000;
    if (idx == 0) return 800 - title.length();
    const QChar prev = title.at(idx - 1);
    const QChar cur  = title.at(idx);
    const bool boundary = prev == ' ' || prev == '-' || prev == '_' ||
                          (prev.isLower() && cur.isUpper());  // 空格/连字符/驼峰边界
    return (boundary ? 600 : 300) - title.length();
}

QList<ResultItem> AppPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();
    if (kw.isEmpty()) return {};

    QList<QPair<int, const ResultItem *>> scored;
    for (const auto &app : m_apps) {
        const int s = matchScore(app.title, kw);
        if (s >= 0) scored.append({s, &app});
    }
    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second->title.compare(b.second->title, Qt::CaseInsensitive) < 0;
    });

    QList<ResultItem> results;
    for (const auto &pr : scored) {
        results.append(*pr.second);
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
// 解析 .lnk 快捷方式的真实目标 exe 路径；失败返回空
static QString resolveLnk(const QString &lnkPath) {
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void **>(&link))))
        return {};

    QString result;
    IPersistFile *pf = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void **>(&pf)))) {
        if (SUCCEEDED(pf->Load(reinterpret_cast<LPCOLESTR>(lnkPath.utf16()),
                               STGM_READ))) {
            wchar_t buf[MAX_PATH] = {0};
            WIN32_FIND_DATAW fd;
            if (SUCCEEDED(link->GetPath(buf, MAX_PATH, &fd, SLGP_RAWPATH)) && buf[0])
                result = QString::fromWCharArray(buf);
        }
        pf->Release();
    }
    link->Release();
    return result;
}

static void scanDir(const QString &path, QList<ResultItem> &out) {
    static QFileIconProvider iconProvider;  // QApplication 已存在，安全
    QDir dir(path);
    for (const QFileInfo &fi : dir.entryInfoList({"*.lnk"}, QDir::Files)) {
        ResultItem item;
        item.title = fi.completeBaseName();

        // 解析失败（UWP/Store/.url/文件夹快捷方式）则回退到 .lnk 本身
        const QString target  = resolveLnk(fi.absoluteFilePath());
        const QString iconSrc = target.isEmpty() ? fi.absoluteFilePath() : target;

        item.action   = iconSrc;  // ShellExecuteW 对 exe 与 .lnk 均有效
        item.subtitle = iconSrc;  // 第二行显示真实路径
        item.icon     = iconProvider.icon(QFileInfo(iconSrc));
        out.append(item);
    }
    for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        scanDir(path + "/" + sub, out);
}

void AppPlugin::loadApps() {
    const bool comOk = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    const QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
        "C:/ProgramData/Microsoft/Windows/Start Menu/Programs"
    };
    for (const QString &root : roots)
        scanDir(root, m_apps);
    if (comOk)
        CoUninitialize();
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
            if (!item.title.isEmpty() && !item.action.isEmpty()) {
                item.subtitle = item.action;  // 第二行显示 Exec 命令
                m_apps.append(item);
            }
        }
    }
}
#endif

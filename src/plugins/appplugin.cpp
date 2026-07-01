#include "appplugin.h"
#include "core/matcher.h"
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#include <QFileIconProvider>
#include <objbase.h>
#include <shlobj.h>
#include <windows.h>
#endif

static constexpr int kReloadDebounceMs = 500; // 装/卸应用会触发多次目录变更，合并后再重扫

AppPlugin::AppPlugin() {
    m_watcher = new QFileSystemWatcher(this);

    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(kReloadDebounceMs);
    connect(m_reloadTimer, &QTimer::timeout, this, &AppPlugin::loadApps);
    // 目录变更先攒进去抖定时器，停顿后只重扫一次
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_reloadTimer,
            qOverload<>(&QTimer::start));

    loadApps();
}

// 用最新目录集替换 watcher 的监听列表（先清旧再加新，避免重扫后残留已删目录）
void AppPlugin::updateWatch(const QStringList &dirs) {
    const QStringList old = m_watcher->directories();
    if (!old.isEmpty()) m_watcher->removePaths(old);
    if (!dirs.isEmpty()) m_watcher->addPaths(dirs);
}

QList<ResultItem> AppPlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();
    if (kw.isEmpty()) return {};

    // 只负责筛选与打分；排序与截断由 MainWindow 跨插件统一处理
    // 拼音匹配扣罚：全拼 -50，首字母缩写 -120；保证直接字面命中始终优先。
    static constexpr int kFullPenalty = 50;
    static constexpr int kInitPenalty = 120;

    QList<ResultItem> results;
    for (int i = 0; i < m_apps.size(); ++i) {
        const ResultItem     &app = m_apps[i];
        const Pinyin::Result &py  = m_pinyins[i];

        // ① 直接字面匹配（优先，无扣罚）
        int  bestScore   = Matcher::score(app.title, kw);
        bool usePinyin   = false;
        bool useInitials = false;

        // ② 全拼匹配（仅在能提升分数时采用）
        if (!py.full.isEmpty()) {
            const int s = Matcher::score(py.full, kw) - kFullPenalty;
            if (s > bestScore) {
                bestScore   = s;
                usePinyin   = true;
                useInitials = false;
            }
        }

        // ③ 首字母缩写匹配（同上）
        if (!py.initials.isEmpty()) {
            const int s = Matcher::score(py.initials, kw) - kInitPenalty;
            if (s > bestScore) {
                bestScore   = s;
                usePinyin   = true;
                useInitials = true;
            }
        }

        if (bestScore < 0) continue;

        ResultItem item = app;
        item.score      = bestScore;
        if (usePinyin)
            item.matchHighlight =
                Pinyin::pinyinMatchedTitlePositions(py, kw, useInitials);
        results.append(item);
    }
    return results;
}

void AppPlugin::execute(const ResultItem &item) {
#ifdef Q_OS_WIN
    ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(item.action.utf16()),
                  nullptr, nullptr, SW_SHOW);
#else
    // action 存的是 Exec 字段，去掉 %u %f 等占位符
    QString cmd = item.action;
    cmd.remove(QRegularExpression("%[uUfFdDnNickvm]"));
    QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
#endif
}

// Ctrl+Enter：把路径/命令复制到剪贴板（跨平台一致）
void AppPlugin::executeAlt(const ResultItem &item) {
    if (!item.action.isEmpty()) QGuiApplication::clipboard()->setText(item.action);
}

// ── 私有：预加载 ──────────────────────────────────────────────

#ifdef Q_OS_WIN
// 解析 .lnk 快捷方式的真实目标 exe 路径；失败返回空
static QString resolveLnk(const QString &lnkPath) {
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void **>(&link))))
        return {};

    QString       result;
    IPersistFile *pf = nullptr;
    if (SUCCEEDED(
            link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&pf)))) {
        if (SUCCEEDED(
                pf->Load(reinterpret_cast<LPCOLESTR>(lnkPath.utf16()), STGM_READ))) {
            wchar_t          buf[MAX_PATH] = {0};
            WIN32_FIND_DATAW fd;
            if (SUCCEEDED(link->GetPath(buf, MAX_PATH, &fd, SLGP_RAWPATH)) && buf[0])
                result = QString::fromWCharArray(buf);
        }
        pf->Release();
    }
    link->Release();
    return result;
}

static void scanDir(const QString &path, QList<ResultItem> &out, QStringList &watch) {
    static QFileIconProvider iconProvider; // QApplication 已存在，安全
    QDir                     dir(path);
    if (!dir.exists()) return;
    watch.append(path); // 监听本目录，子目录新增/删除（即装卸应用）会触发重扫
    for (const QFileInfo &fi : dir.entryInfoList({"*.lnk"}, QDir::Files)) {
        ResultItem item;
        item.title = fi.completeBaseName();

        // 解析失败（UWP/Store/.url/文件夹快捷方式）则回退到 .lnk 本身
        const QString target  = resolveLnk(fi.absoluteFilePath());
        const QString iconSrc = target.isEmpty() ? fi.absoluteFilePath() : target;

        item.action   = iconSrc; // ShellExecuteW 对 exe 与 .lnk 均有效
        item.subtitle = iconSrc; // 第二行显示真实路径
        item.icon     = iconProvider.icon(QFileInfo(iconSrc));
        out.append(item);
    }
    for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        scanDir(path + "/" + sub, out, watch);
}

void AppPlugin::loadApps() {
    m_apps.clear();
    m_pinyins.clear();
    const bool comOk = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    const QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
        "C:/ProgramData/Microsoft/Windows/Start Menu/Programs"};
    QStringList watch;
    for (const QString &root : roots)
        scanDir(root, m_apps, watch);
    for (const ResultItem &app : m_apps)
        m_pinyins.append(Pinyin::compute(app.title));
    if (comOk) CoUninitialize();
    updateWatch(watch);
}

#else
static ResultItem parseDesktop(const QString &path) {
    ResultItem item;
    QFile      f(path);
    if (!f.open(QIODevice::ReadOnly)) return item;

    bool inEntry = false;
    for (QTextStream ts(&f); !ts.atEnd();) {
        const QString line = ts.readLine().trimmed();
        if (line == "[Desktop Entry]") {
            inEntry = true;
            continue;
        }
        if (line.startsWith('[')) {
            inEntry = false;
            continue;
        }
        if (!inEntry) continue;

        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();

        if (key == "Name" && item.title.isEmpty()) item.title = val;
        else if (key == "Exec" && item.action.isEmpty()) item.action = val;
        else if (key == "Icon")
            item.icon =
                QIcon::fromTheme(val, QIcon::fromTheme("application-x-executable"));
        else if (key == "NoDisplay" && val == "true") {
            item.title.clear();
            break;
        }
    }
    return item;
}

void AppPlugin::loadApps() {
    m_apps.clear();
    m_pinyins.clear();
    const QStringList dirs = {"/usr/share/applications",
                              QDir::homePath() + "/.local/share/applications"};
    QStringList       watch;
    for (const QString &d : dirs) {
        if (!QDir(d).exists()) continue;
        watch.append(d);
        for (const QFileInfo &fi : QDir(d).entryInfoList({"*.desktop"}, QDir::Files)) {
            ResultItem item = parseDesktop(fi.absoluteFilePath());
            if (!item.title.isEmpty() && !item.action.isEmpty()) {
                item.subtitle = item.action; // 第二行显示 Exec 命令
                m_apps.append(item);
            }
        }
    }
    for (const ResultItem &app : m_apps)
        m_pinyins.append(Pinyin::compute(app.title));
    updateWatch(watch);
}
#endif

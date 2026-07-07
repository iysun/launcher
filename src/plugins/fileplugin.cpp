#include "fileplugin.h"
#include "core/appsettings.h"
#include "core/matcher.h"
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

// 同步遍历在 UI 线程执行，加硬上限避免大目录卡顿：
static constexpr int kMaxVisit   = 8000; // 访问条目数封顶，超出即停
static constexpr int kMaxResults = 200;  // 命中数封顶（MainWindow 还会再截到 kMaxItems）

FilePlugin::FilePlugin(AppSettings *settings) : m_settings(settings) {}

// 无设置对象时的兜底默认目录，去重（部分平台上某些 Location 可能落到同一目录）
QStringList FilePlugin::searchRoots() {
    QStringList roots;
    const auto  types = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::DownloadLocation,
        QStandardPaths::PicturesLocation,
    };
    for (auto t : types) {
        const QString dir = QStandardPaths::writableLocation(t);
        if (!dir.isEmpty() && !roots.contains(dir)) roots.append(dir);
    }
    return roots;
}

// 注意：本函数在工作线程执行（runsAsync），遍历文件名 + 打分，可重入。搜索根来自
// AppSettings::fileSearchPaths()（内部加锁，可跨线程安全读取）——m_settings 非空时尊重
// 用户配置（包括显式清空为"不搜索"），为空才回退到 searchRoots() 的默认目录。
// **不要**在此创建 QIcon/QPixmap（仅限 GUI 线程）——图标交 decorate 补。
QList<ResultItem> FilePlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();
    if (kw.isEmpty()) return {};

    QList<ResultItem> results;
    int               visited = 0;

    const QStringList roots = m_settings ? m_settings->fileSearchPaths() : searchRoots();

    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (++visited > kMaxVisit) return results; // 访问上限，早退避免后台堆积

            const QFileInfo fi = it.fileInfo();
            const int       s  = Matcher::score(fi.fileName(), kw);
            if (s < 0) continue;

            ResultItem item;
            item.title    = fi.fileName();
            item.subtitle = fi.absoluteFilePath();
            item.action   = fi.absoluteFilePath();
            item.score    = s;
            results.append(item);
            if (results.size() >= kMaxResults) return results;
        }
    }
    return results;
}

// 主线程调用：仅对将展示的结果按路径补图标（QFileIconProvider 内部建 QPixmap，须在 GUI
// 线程）
void FilePlugin::decorate(ResultItem &item) {
    static QFileIconProvider iconProvider;
    item.icon = iconProvider.icon(QFileInfo(item.action));
}

void FilePlugin::execute(const ResultItem &item) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(item.action));
}

// Ctrl+Enter：打开文件所在目录
void FilePlugin::executeAlt(const ResultItem &item) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(item.action).absolutePath()));
}

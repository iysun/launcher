#include "fileplugin.h"
#include "core/matcher.h"
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

// 同步遍历在 UI 线程执行，加硬上限避免大目录卡顿：
static constexpr int kMaxVisit   = 8000;  // 访问条目数封顶，超出即停
static constexpr int kMaxResults = 200;   // 命中数封顶（MainWindow 还会再截到 kMaxItems）

QStringList FilePlugin::searchRoots() {
    // 去重：部分平台上某些 Location 可能落到同一目录
    QStringList roots;
    const auto types = {
        QStandardPaths::DesktopLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::DownloadLocation,
        QStandardPaths::PicturesLocation,
    };
    for (auto t : types) {
        const QString dir = QStandardPaths::writableLocation(t);
        if (!dir.isEmpty() && !roots.contains(dir))
            roots.append(dir);
    }
    return roots;
}

QList<ResultItem> FilePlugin::query(const QString &keyword) {
    const QString kw = keyword.trimmed();
    if (kw.isEmpty()) return {};

    static QFileIconProvider iconProvider;  // QApplication 已存在，安全
    QList<ResultItem> results;
    int visited = 0;

    for (const QString &root : searchRoots()) {
        QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (++visited > kMaxVisit) return results;  // 访问上限，早退避免卡死

            const QFileInfo fi = it.fileInfo();
            const int s = Matcher::score(fi.fileName(), kw);
            if (s < 0) continue;

            ResultItem item;
            item.title    = fi.fileName();
            item.subtitle = fi.absoluteFilePath();
            item.action   = fi.absoluteFilePath();
            item.icon     = iconProvider.icon(fi);
            item.score    = s;
            results.append(item);
            if (results.size() >= kMaxResults) return results;
        }
    }
    return results;
}

void FilePlugin::execute(const ResultItem &item) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(item.action));
}

// Ctrl+Enter：打开文件所在目录
void FilePlugin::executeAlt(const ResultItem &item) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(item.action).absolutePath()));
}

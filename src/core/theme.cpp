#include "core/theme.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QStandardPaths>
#include <QStyleHints>
#include <QVector>

Theme &Theme::instance() {
    static Theme inst;
    return inst;
}

QString Theme::themesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
}

QString Theme::cacheDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/theme-cache";
}

QHash<QString, QString> Theme::loadColors(const QString &jsonPath) {
    QHash<QString, QString> result;
    QFile                    f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    const QJsonObject root   = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject colors = root["colors"].toObject();
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it)
        result.insert(it.key(), it.value().toString());
    return result;
}

// 读主题 JSON 顶层的可选 `ansi` 数组（16 个 hex 字符串）。缺失/非数组返回空列表，
// 由 ansiPalette() 负责回退填满；非法颜色项以无效 QColor 占位，同样在填满时被替换。
QList<QColor> Theme::loadAnsi(const QString &jsonPath) {
    QList<QColor> result;
    QFile         f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object()["ansi"].toArray();
    for (const QJsonValue &v : arr)
        result.append(QColor(v.toString()));
    return result;
}

// 把内置 :/themes/{mocha,latte,dracula,nord}.json 落盘到 datadir/themes/，仅当目标文件
// 不存在时才写入，不用 QFile::copy 是为了避免继承 Qt 资源文件的只读属性——用户需要能
// 直接编辑/复制这些文件（与 I18n::ensureDefaultFiles 逐字对应）
void Theme::ensureDefaultFiles() const {
    const QString dir = themesDir();
    QDir().mkpath(dir);

    for (const char *code : {"mocha", "latte", "dracula", "nord"}) {
        const QString target = dir + "/" + code + ".json";
        if (QFile::exists(target)) continue;

        QFile src(QString(":/themes/%1.json").arg(code));
        if (!src.open(QIODevice::ReadOnly)) continue;
        const QByteArray content = src.readAll();

        QFile out(target);
        if (out.open(QIODevice::WriteOnly)) out.write(content);
    }
}

static void paintPolylineIcon(const QString &path, int size, const QColor &color,
                               const QVector<QPointF> &pts) {
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPolyline(pts.constData(), pts.size());
    p.end();

    img.save(path, "PNG");
}

// 勾选框对勾 / 下拉箭头这两个 QSS `image: url(...)` 引用的图标不能再编译期烘焙
// （主题现在是数据驱动、用户可任意新增，没法为每个自定义主题预生成 PNG）。改为
// 启动时按当前主题色现画，落盘到 datadir/theme-cache/，QSS 用绝对路径引用——
// Qt 样式表的 url() 支持磁盘绝对路径，不要求一定是 qrc 资源。坐标/描边宽度与原
// tools/gen_check_icon.py / gen_chevron_icon.py 保持一致，保证视觉不变。
void Theme::generateIcons() const {
    const QString dir = cacheDir();
    QDir().mkpath(dir);
    paintPolylineIcon(dir + "/check.png", 16, color("bg"),
                       {{3, 8}, {6.5, 11.5}, {13, 4}});
    paintPolylineIcon(dir + "/chevron-down.png", 12, color("overlay"),
                       {{2, 4}, {6, 8.5}, {10, 4}});
}

// appearanceMode 为 "dark"/"light" 时直接取对应的 code；为 "system" 时查一次系统深浅色
// 偏好折算——Qt::ColorScheme::Unknown（部分 Linux 桌面环境不支持系统级查询）按 dark 兜底。
QString Theme::resolveCode(const QString &appearanceMode, const QString &darkCode,
                           const QString &lightCode) const {
    QString mode = appearanceMode;
    if (mode == "system") {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        mode                         = scheme == Qt::ColorScheme::Light ? "light" : "dark";
    }
    return mode == "light" ? lightCode : darkCode;
}

void Theme::init(const QString &appearanceMode, const QString &darkCode, const QString &lightCode) {
    ensureDefaultFiles();

    const QString resolved = resolveCode(appearanceMode, darkCode, lightCode);
    m_code                 = resolved.isEmpty() ? "mocha" : resolved;
    m_mochaFallback        = loadColors(":/themes/mocha.json");
    m_mochaAnsi            = loadAnsi(":/themes/mocha.json");

    m_colors = loadColors(themesDir() + "/" + m_code + ".json");
    m_ansi   = loadAnsi(themesDir() + "/" + m_code + ".json");
    if (m_colors.isEmpty()) { // 内置主题的 datadir 副本理论上落盘后必然存在；此处兜底覆盖异常场景
        m_colors = loadColors(QString(":/themes/%1.json").arg(m_code));
        m_ansi   = loadAnsi(QString(":/themes/%1.json").arg(m_code));
    }
    if (m_ansi.isEmpty()) // datadir 里是老版本（无 ansi 字段）的内置主题：回退到其资源副本
        m_ansi = loadAnsi(QString(":/themes/%1.json").arg(m_code));
    if (m_colors.isEmpty()) // 自定义主题文件损坏/缺失时的最终兜底，保证界面至少可见
        m_colors = m_mochaFallback;

    generateIcons(); // 必须在 m_colors 就绪后调用
}

QString Theme::colorFor(const QString &role) const {
    if (const auto it = m_colors.constFind(role); it != m_colors.constEnd()) return it.value();
    if (const auto it = m_mochaFallback.constFind(role); it != m_mochaFallback.constEnd())
        return it.value();
    return "#ff00ff"; // 显眼的洋红兜底：角色缺失时一眼看出，而不是隐形/崩溃
}

QString Theme::c(const QString &role) {
    return instance().colorFor(role);
}

QColor Theme::color(const QString &role) {
    return QColor(instance().colorFor(role));
}

QList<QColor> Theme::ansiPalette() {
    // 最终兜底：标准 xterm ANSI 16 色（当前主题与内置 mocha 都没提供 ansi 时用）
    static const char *kDefault[16] = {
        "#000000", "#cd0000", "#00cd00", "#cdcd00", "#0000ee", "#cd00cd",
        "#00cdcd", "#e5e5e5", "#7f7f7f", "#ff0000", "#00ff00", "#ffff00",
        "#5c5cff", "#ff00ff", "#00ffff", "#ffffff"};

    const Theme        &inst = instance();
    QList<QColor>       out;
    out.reserve(16);
    for (int i = 0; i < 16; ++i) {
        // 当前主题 → 内置 mocha → 标准默认；任一层无效则继续回退
        if (i < inst.m_ansi.size() && inst.m_ansi[i].isValid())
            out.append(inst.m_ansi[i]);
        else if (i < inst.m_mochaAnsi.size() && inst.m_mochaAnsi[i].isValid())
            out.append(inst.m_mochaAnsi[i]);
        else
            out.append(QColor(kDefault[i]));
    }
    return out;
}

QString Theme::checkIconPath() {
    return QDir::fromNativeSeparators(cacheDir() + "/check.png");
}

QString Theme::chevronIconPath() {
    return QDir::fromNativeSeparators(cacheDir() + "/chevron-down.png");
}

QList<QPair<QString, QString>> Theme::availableThemes(const QString &appearance) const {
    QList<QPair<QString, QString>> result;
    QDir                             dir(themesDir());
    for (const QString &fileName : dir.entryList({"*.json"}, QDir::Files)) {
        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object()["meta"].toObject();
        const QString     code = meta["code"].toString();
        const QString     name = meta["name"].toString();
        if (code.isEmpty() || name.isEmpty()) continue; // 缺元信息的文件跳过，避免下拉框出现空项

        // 缺失/非法 appearance 一律按 dark 归类，保证主题不会从任一下拉框里隐形消失
        QString themeAppearance = meta["appearance"].toString();
        if (themeAppearance != "dark" && themeAppearance != "light") themeAppearance = "dark";
        if (!appearance.isEmpty() && themeAppearance != appearance) continue;

        result.append({code, name});
    }
    return result;
}

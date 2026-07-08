#pragma once
#include <QColor>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

// 极简主题系统，形状镜像 I18n：主题包是 datadir/themes/<code>.json
// （meta.code/meta.name + 一张扁平的颜色角色表）。内置 mocha/latte/dracula/nord
// 通过 Qt 资源 (:/themes/<code>.json) 首次运行时落盘到 datadir，用户可在该目录
// 直接编辑或新增自定义主题文件。主题切换只在启动时 init() 一次生效，不做运行时
// 热切换（与语言切换同一约束），故本类无需 QObject/信号。
class Theme {
public:
    static Theme &instance();

    // 启动时调用一次：确保内置主题文件已落盘到 datadir，加载 themeCode 对应的
    // 主题，并按当前主题色重新生成勾选框对勾/下拉箭头这两个运行时图标缓存文件。
    void init(const QString &themeCode);

    QString themeCode() const { return m_code; }

    // 查表：当前主题 → 内置 mocha 兜底 → 醒目的洋红（角色缺失时一眼看出，而不是隐形崩溃）
    static QString c(const QString &role);     // QSS 拼接用："#rrggbb"
    static QColor  color(const QString &role); // QPainter 用

    // 扫描 datadir/themes/*.json，返回 (code, displayName) 列表，供设置页主题下拉框使用
    QList<QPair<QString, QString>> availableThemes() const;

    // 供 QSS `image: url(...)` 引用的运行时着色图标绝对路径
    static QString checkIconPath();
    static QString chevronIconPath();

private:
    QString colorFor(const QString &role) const;
    void    ensureDefaultFiles() const;
    void    generateIcons() const;
    static QHash<QString, QString> loadColors(const QString &jsonPath);
    static QString                 themesDir();
    static QString                 cacheDir();

    QHash<QString, QString> m_colors;        // 当前主题
    QHash<QString, QString> m_mochaFallback; // 内置 mocha 兜底，永远从 :/themes/mocha.json 加载
    QString                 m_code = "mocha";
};

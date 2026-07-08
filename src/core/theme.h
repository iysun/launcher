#pragma once
#include <QColor>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

// 极简主题系统，形状镜像 I18n：主题包是 datadir/themes/<code>.json
// （meta.code/meta.name/meta.appearance + 一张扁平的颜色角色表）。内置
// mocha/latte/dracula/nord 通过 Qt 资源 (:/themes/<code>.json) 首次运行时落盘到
// datadir，用户可在该目录直接编辑或新增自定义主题文件。
//
// 外观分深色/浅色/跟随系统三档（类 Zed 设计）：用户分别为深色、浅色配置一个默认
// 主题，"跟随系统"时按启动那一刻的系统偏好折算成 dark/light 二选一。主题切换（含
// 外观模式切换）只在启动时 init() 一次生效，不做运行时热切换（与语言切换同一
// 约束），故本类无需 QObject/信号。
class Theme {
public:
    static Theme &instance();

    // 启动时调用一次：确保内置主题文件已落盘到 datadir；appearanceMode 为
    // "dark"/"light" 时直接用对应的 darkCode/lightCode，为 "system" 时用
    // QGuiApplication::styleHints()->colorScheme() 查一次系统偏好折算（查不到时
    // 按 dark 兜底），再加载解析出的主题，并按其颜色重新生成勾选框对勾/下拉箭头
    // 这两个运行时图标缓存文件。
    void init(const QString &appearanceMode, const QString &darkCode, const QString &lightCode);

    QString themeCode() const { return m_code; } // 解析后实际生效的主题 code

    // 查表：当前主题 → 内置 mocha 兜底 → 醒目的洋红（角色缺失时一眼看出，而不是隐形崩溃）
    static QString c(const QString &role);     // QSS 拼接用："#rrggbb"
    static QColor  color(const QString &role); // QPainter 用

    // 扫描 datadir/themes/*.json，返回 (code, displayName) 列表，供设置页下拉框使用。
    // appearance 非空时按 meta.appearance 过滤（"dark"/"light"）；主题文件缺失/
    // 非法该字段一律按 "dark" 归类，不会导致主题从任何下拉框里消失。
    QList<QPair<QString, QString>> availableThemes(const QString &appearance = QString()) const;

    // 供 QSS `image: url(...)` 引用的运行时着色图标绝对路径
    static QString checkIconPath();
    static QString chevronIconPath();

private:
    QString colorFor(const QString &role) const;
    void    ensureDefaultFiles() const;
    void    generateIcons() const;
    QString resolveCode(const QString &appearanceMode, const QString &darkCode,
                        const QString &lightCode) const;
    static QHash<QString, QString> loadColors(const QString &jsonPath);
    static QString                 themesDir();
    static QString                 cacheDir();

    QHash<QString, QString> m_colors;        // 当前主题
    QHash<QString, QString> m_mochaFallback; // 内置 mocha 兜底，永远从 :/themes/mocha.json 加载
    QString                 m_code = "mocha";
};

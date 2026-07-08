#include "core/theme.h"
#include "core/usagestore.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtTest>

// UsageStore（frecency）与 Theme（外观模式/颜色兜底链/主题分类）的单测。
// Theme::init() 内部会走 QGuiApplication::styleHints()（system 模式解析）与 QPainter
// （现画图标），故本目标用 QGuiApplication（offscreen 平台），而非 matcher_test 的 guiless。
// QStandardPaths::setTestModeEnabled(true) 把 AppDataLocation 重定向到测试沙箱，
// 与用户真实的 settings/themes 目录隔离，互不污染。
class TestCore : public QObject {
    Q_OBJECT
private:
    static QString themesDir() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
    }
    static void writeTheme(const QString &file, const QByteArray &json) {
        QDir().mkpath(themesDir());
        QFile f(themesDir() + "/" + file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(json);
    }

private slots:
    void initTestCase();

    // ── UsageStore ──
    void frecencyUnknownKeyIsZero();
    void frecencyFreshUseIsPositive();
    void frecencyIsCappedAndBounded();

    // ── Theme 外观模式解析 ──
    void themeDarkModeResolvesDarkCode();
    void themeLightModeResolvesLightCode();
    void themeDarkModeHonorsChosenDarkTheme();
    void themeSystemModeResolvesToOneOfThem();

    // ── Theme 颜色兜底链 ──
    void themeKnownRoleReturnsThemeColor();
    void themeMissingRoleFallsBackToMocha();
    void themeUnknownRoleReturnsMagenta();

    // ── Theme 按外观分类 ──
    void themeAvailableFilteredByAppearance();
    void themeMissingAppearanceClassifiedDark();
};

void TestCore::initTestCase() {
    // 清空测试沙箱，保证可复现（不影响用户真实 datadir，已被 setTestModeEnabled 隔离）
    const QString datadir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(datadir).removeRecursively();
}

// ── UsageStore ────────────────────────────────────────────────

void TestCore::frecencyUnknownKeyIsZero() {
    UsageStore u;
    QCOMPARE(u.frecencyBonus("never-used-key"), 0);
}

void TestCore::frecencyFreshUseIsPositive() {
    UsageStore u;
    u.recordUse("fresh-key");
    // 刚用过：recency=30（<1天）+ countW=min(1*3,30)=3 = 33
    QCOMPARE(u.frecencyBonus("fresh-key"), 33);
}

void TestCore::frecencyIsCappedAndBounded() {
    UsageStore u;
    for (int i = 0; i < 50; ++i) u.recordUse("hot-key");
    // countW 封顶 30 + recency 30 = 60，且总量被 kFrecencyCap=60 钳制，不会越过匹配档间距
    QCOMPARE(u.frecencyBonus("hot-key"), 60);
}

// ── Theme 外观模式解析 ────────────────────────────────────────

void TestCore::themeDarkModeResolvesDarkCode() {
    Theme::instance().init("dark", "mocha", "latte");
    QCOMPARE(Theme::instance().themeCode(), QString("mocha"));
    QCOMPARE(Theme::c("bg"), QString("#1e1e2e"));
}

void TestCore::themeLightModeResolvesLightCode() {
    Theme::instance().init("light", "mocha", "latte");
    QCOMPARE(Theme::instance().themeCode(), QString("latte"));
    QCOMPARE(Theme::c("bg"), QString("#eff1f5"));
}

void TestCore::themeDarkModeHonorsChosenDarkTheme() {
    Theme::instance().init("dark", "nord", "latte");
    QCOMPARE(Theme::instance().themeCode(), QString("nord"));
    QCOMPARE(Theme::c("bg"), QString("#2e3440"));
}

void TestCore::themeSystemModeResolvesToOneOfThem() {
    Theme::instance().init("system", "nord", "latte");
    // 具体取哪个取决于运行环境的系统深浅色偏好（offscreen 下通常 Unknown → 兜底 dark），
    // 但结果必是所配置的深色/浅色二者之一，且不崩溃
    const QString code = Theme::instance().themeCode();
    QVERIFY2(code == "nord" || code == "latte", qPrintable(code));
}

// ── Theme 颜色兜底链 ──────────────────────────────────────────

void TestCore::themeKnownRoleReturnsThemeColor() {
    Theme::instance().init("dark", "mocha", "latte");
    QCOMPARE(Theme::c("accent"), QString("#89b4fa"));
}

void TestCore::themeMissingRoleFallsBackToMocha() {
    // 自定义主题只给了 bg，其余角色缺失 → 应回退到内置 mocha 的对应色
    writeTheme("partial.json",
               R"({"meta":{"code":"partial","name":"Partial","appearance":"dark"},
                   "colors":{"bg":"#101010"}})");
    Theme::instance().init("dark", "partial", "latte");
    QCOMPARE(Theme::instance().themeCode(), QString("partial"));
    QCOMPARE(Theme::c("bg"), QString("#101010"));      // 自带的角色照常
    QCOMPARE(Theme::c("danger"), QString("#f38ba8"));  // 缺失角色 → 回退到 mocha 的 danger
}

void TestCore::themeUnknownRoleReturnsMagenta() {
    Theme::instance().init("dark", "mocha", "latte");
    // 连内置 mocha 都没有的角色名 → 醒目洋红兜底（便于一眼发现，而不是隐形/崩溃）
    QCOMPARE(Theme::c("totally-made-up-role"), QString("#ff00ff"));
}

// ── Theme 按外观分类 ──────────────────────────────────────────

void TestCore::themeAvailableFilteredByAppearance() {
    Theme::instance().init("dark", "mocha", "latte"); // 确保内置四套已落盘
    QStringList darkCodes, lightCodes;
    for (const auto &p : Theme::instance().availableThemes("dark")) darkCodes << p.first;
    for (const auto &p : Theme::instance().availableThemes("light")) lightCodes << p.first;

    QVERIFY(darkCodes.contains("mocha"));
    QVERIFY(darkCodes.contains("dracula"));
    QVERIFY(darkCodes.contains("nord"));
    QVERIFY(!darkCodes.contains("latte"));

    QVERIFY(lightCodes.contains("latte"));
    QVERIFY(!lightCodes.contains("mocha"));
}

void TestCore::themeMissingAppearanceClassifiedDark() {
    // meta 里没有 appearance 字段 → 约定归类为 dark，不从下拉框消失
    writeTheme("noapp.json",
               R"({"meta":{"code":"noapp","name":"NoApp"},"colors":{"bg":"#222222"}})");
    QStringList darkCodes, lightCodes;
    for (const auto &p : Theme::instance().availableThemes("dark")) darkCodes << p.first;
    for (const auto &p : Theme::instance().availableThemes("light")) lightCodes << p.first;
    QVERIFY(darkCodes.contains("noapp"));
    QVERIFY(!lightCodes.contains("noapp"));
}

// offscreen 平台 + QStandardPaths 测试模式：init 早于 QGuiApplication 前用 qputenv 设置，
// 保证无显示环境（CI/headless）也能跑 QPainter/styleHints。
int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("launcher_core_test");
    QStandardPaths::setTestModeEnabled(true);
    TestCore tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_core.moc"

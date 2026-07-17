#include "mainwindow.h"
#include "core/appsettings.h"
#include "core/i18n.h"
#include "core/theme.h"
#include "plugins/appplugin.h"
#include "plugins/commandplugin.h"
#include "plugins/fileplugin.h"
#include "plugins/runplugin.h"
#include "plugins/webplugin.h"
#include "ui/settingsdialog.h"
#include "ui/terminalwindow.h"
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(
        "launcher"); // 决定 usage.json / settings.json 目录，须早于 MainWindow

    auto *settings = new AppSettings;
    I18n::instance().init(settings->language()); // 须早于任何窗口构造，UI 文案才能取到正确语言
    // 同理，须早于任何窗口构造，颜色才能取到正确主题；system 外观在此按启动时的系统偏好解析一次
    Theme::instance().init(settings->appearanceMode(), settings->darkTheme(), settings->lightTheme());

    MainWindow win(settings);

    auto *appPlugin = new AppPlugin;
    win.addPlugin(appPlugin);               // 全局：应用搜索
    win.addPlugin(new FilePlugin(settings)); // "@" 前缀：文件搜索
    win.addPlugin(new WebPlugin(settings)); // "?" 前缀：网页搜索

    // "/" 前缀：launcher 自身命令。动作在此装配，命令插件本身不与具体能力耦合。
    auto *cmd = new CommandPlugin; // help 命令已内置自注册
    cmd->addCommand("quit", I18n::t("cmd.quit"), [] { qApp->quit(); });
    cmd->addCommand("reload", I18n::t("cmd.reload"), [appPlugin] { appPlugin->reload(); });
    cmd->addCommand("datadir", I18n::t("cmd.datadir"), [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    });
    cmd->addCommand("restart", I18n::t("cmd.restart"), [] {
        QProcess::startDetached(QApplication::applicationFilePath());
        qApp->quit();
    });
    // "/terminal"：在 launcher 内联展开终端模式（融合入口）；卡片морф为内嵌交互终端
    cmd->addCommand("terminal", I18n::t("cmd.terminal"), [&win] { win.enterTerminal(); });
    // "/termwin"：打开独立可缩放终端窗口（libvterm + ConPTY），单例复用，懒启动 shell
    auto *terminalWin = new TerminalWindow;
    cmd->addCommand("termwin", I18n::t("cmd.terminalWindow"), [terminalWin] {
        terminalWin->show();
        terminalWin->raise();
        terminalWin->activateWindow();
        terminalWin->startSession();
    });

    win.addPlugin(cmd);

    // ":" 前缀：在内联终端执行命令（`: ipconfig`）。进入终端模式并跑该命令。
    win.addPlugin(new RunPlugin([&win](const QString &c) { win.enterTerminal(c); }));

    // 所有插件注册完毕后创建设置对话框（插件列表已完整）
    auto *settingsDialog = new SettingsDialog(settings, win.plugins());
    cmd->addCommand("settings", I18n::t("cmd.settings"), [settingsDialog] {
        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
    });

    // win.show();

    return app.exec();
}

#include "mainwindow.h"
#include "core/appsettings.h"
#include "plugins/appplugin.h"
#include "plugins/commandplugin.h"
#include "plugins/fileplugin.h"
#include "plugins/webplugin.h"
#include "ui/settingsdialog.h"
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("launcher");  // 决定 usage.json / settings.json 目录，须早于 MainWindow

    auto *settings = new AppSettings;
    MainWindow win(settings);

    auto *appPlugin = new AppPlugin;
    win.addPlugin(appPlugin);       // 全局：应用搜索
    win.addPlugin(new FilePlugin);  // "@" 前缀：文件搜索
    win.addPlugin(new WebPlugin(settings));   // "?" 前缀：网页搜索

    // "/" 前缀：launcher 自身命令。动作在此装配，命令插件本身不与具体能力耦合。
    auto *cmd = new CommandPlugin;  // help 命令已内置自注册
    cmd->addCommand("quit", "退出 launcher", [] { qApp->quit(); });
    cmd->addCommand("reload", "重新加载应用列表",
                    [appPlugin] { appPlugin->reload(); });
    cmd->addCommand("datadir", "打开配置 / 数据目录", [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    });
    cmd->addCommand("restart", "重启 launcher", [] {
        QProcess::startDetached(QApplication::applicationFilePath());
        qApp->quit();
    });
    win.addPlugin(cmd);

    // 所有插件注册完毕后创建设置对话框（插件列表已完整）
    auto *settingsDialog = new SettingsDialog(settings, win.plugins());
    cmd->addCommand("settings", "打开设置", [settingsDialog] {
        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
    });

    win.show();

    return app.exec();
}

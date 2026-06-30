#include "mainwindow.h"
#include "plugins/appplugin.h"
#include "plugins/commandplugin.h"
#include "plugins/fileplugin.h"
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("launcher");  // 决定 usage.json 等用户数据目录，须早于 MainWindow

    MainWindow win;

    auto *appPlugin = new AppPlugin;
    win.addPlugin(appPlugin);     // 全局：应用搜索
    win.addPlugin(new FilePlugin);  // "@" 前缀：文件搜索

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

    win.show();

    return app.exec();
}

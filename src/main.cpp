#include "mainwindow.h"
#include "plugins/appplugin.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("launcher");  // 决定 usage.json 等用户数据目录，须早于 MainWindow

    MainWindow win;
    win.addPlugin(new AppPlugin);
    win.show();

    return app.exec();
}

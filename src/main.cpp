#include "mainwindow.h"
#include "plugins/appplugin.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);  // 关闭窗口不退出

    MainWindow win;
    win.addPlugin(new AppPlugin);
    // win.addPlugin(new CalcPlugin);     // 后续扩展

    // 首次启动显示一次，之后通过热键 Alt+Space 唤起
    win.show();

    return app.exec();
}

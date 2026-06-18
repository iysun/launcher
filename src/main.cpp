#include "mainwindow.h"
#include "plugins/appplugin.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    MainWindow win;
    win.addPlugin(new AppPlugin);
    win.show();

    return app.exec();
}

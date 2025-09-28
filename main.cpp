#include "mainwindow.h"
#include <QApplication>
#include "androidutils.h"
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QTimer::singleShot(0, [](){
        AndroidUtils::applyImmersive(true);
        AndroidUtils::keepScreenOn(true);
    });
    return a.exec();
}

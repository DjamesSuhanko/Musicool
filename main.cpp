#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#ifdef Q_OS_ANDROID
#include <QtCore/qnativeinterface.h>
#include <QtCore/QJniObject>
#endif
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(Theme::darkPalette());
    qApp->setStyleSheet(Theme::globalQss());

#ifdef Q_OS_ANDROID
    if (QJniObject activity = QNativeInterface::QAndroidApplication::context(); activity.isValid()) {
        // Instala edge-to-edge e insets diretamente no view do Qt (mesmo se criado depois)
        QJniObject::callStaticMethod<void>(
            "org/qtproject/example/EdgeToEdgeHelper",
            "installForQt",
            "(Landroid/app/Activity;)V",
            activity.object<jobject>());

        // Mantém a tela ligada (FLAG + WakeLock) enquanto em foreground
        QJniObject::callStaticMethod<void>(
            "org/qtproject/example/EdgeToEdgeHelper",
            "keepScreenOn",
            "(Landroid/app/Activity;Z)V",
            activity.object<jobject>(), true);
    }
#endif

    MainWindow w;
    w.show();

#ifdef Q_OS_ANDROID
    QObject::connect(&a, &QGuiApplication::applicationStateChanged, &w,
                     [](Qt::ApplicationState s){
                         if (s == Qt::ApplicationActive) {
                             if (QJniObject activity = QNativeInterface::QAndroidApplication::context(); activity.isValid()) {
                                 QJniObject::callStaticMethod<void>(
                                     "org/qtproject/example/EdgeToEdgeHelper",
                                     "keepScreenOn",
                                     "(Landroid/app/Activity;Z)V",
                                     activity.object<jobject>(), true);
                                 // força um applyInsets após resume
                                 QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
                                 if (window.isValid()) {
                                     QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
                                     if (decor.isValid()) decor.callMethod<void>("requestApplyInsets", "()V");
                                 }
                             }
                         } else {
                             // opcional: pode soltar o wakelock quando sair da frente
                             if (QJniObject activity = QNativeInterface::QAndroidApplication::context(); activity.isValid()) {
                                 QJniObject::callStaticMethod<void>(
                                     "org/qtproject/example/EdgeToEdgeHelper",
                                     "keepScreenOn",
                                     "(Landroid/app/Activity;Z)V",
                                     activity.object<jobject>(), false);
                             }
                         }
                     });
#endif

    return a.exec();
}

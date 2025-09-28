#include "androidutils.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QApplication>
#include <QtCore/qnativeinterface.h> // torna o code model feliz
#endif

void AndroidUtils::applyImmersive(bool on) {
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod("getWindow","()Landroid/view/Window;");
    if (!window.isValid()) return;

    // Em devices edge-to-edge, isso evita recortes estranhos:
    window.callMethod<void>("setDecorFitsSystemWindows","(Z)V", false);

    const jint sdkInt = QJniObject::getStaticField<jint>("android/os/Build$VERSION","SDK_INT");

    if (on) {
        if (sdkInt >= 30) {
            // API 30+: WindowInsetsController
            QJniObject controller = window.callObjectMethod(
                "getInsetsController","()Landroid/view/WindowInsetsController;");
            if (controller.isValid()) {
                jint statusBars = QJniObject::callStaticMethod<jint>(
                    "android/view/WindowInsets$Type","statusBars","()I");
                jint navBars = QJniObject::callStaticMethod<jint>(
                    "android/view/WindowInsets$Type","navigationBars","()I");
                jint types = statusBars | navBars;

                jint BEHAVIOR = QJniObject::getStaticField<jint>(
                    "android/view/WindowInsetsController",
                    "BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE");
                controller.callMethod<void>("setSystemBarsBehavior","(I)V", BEHAVIOR);
                controller.callMethod<void>("hide","(I)V", types);
            }
        } else {
            // Fallback (pré-Android 11): SYSTEM_UI_FLAG_*
            QJniObject decor = window.callObjectMethod("getDecorView","()Landroid/view/View;");
            if (decor.isValid()) {
                jint flags = 0;
                auto add = [&](const char* n){
                    flags |= QJniObject::getStaticField<jint>("android/view/View", n);
                };
                add("SYSTEM_UI_FLAG_LAYOUT_STABLE");
                add("SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION");
                add("SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN");
                add("SYSTEM_UI_FLAG_HIDE_NAVIGATION");
                add("SYSTEM_UI_FLAG_FULLSCREEN");
                add("SYSTEM_UI_FLAG_IMMERSIVE_STICKY");
                decor.callMethod<void>("setSystemUiVisibility","(I)V", flags);
            }
        }
    } else {
        if (sdkInt >= 30) {
            QJniObject controller = window.callObjectMethod(
                "getInsetsController","()Landroid/view/WindowInsetsController;");
            if (controller.isValid()) {
                jint statusBars = QJniObject::callStaticMethod<jint>(
                    "android/view/WindowInsets$Type","statusBars","()I");
                jint navBars = QJniObject::callStaticMethod<jint>(
                    "android/view/WindowInsets$Type","navigationBars","()I");
                controller.callMethod<void>("show","(I)V", statusBars | navBars);
            }
        } else {
            QJniObject decor = window.callObjectMethod("getDecorView","()Landroid/view/View;");
            if (decor.isValid())
                decor.callMethod<void>("setSystemUiVisibility","(I)V", 0);
        }
    }
#else
    Q_UNUSED(on);
#endif
}

void AndroidUtils::keepScreenOn(bool on) {
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod("getWindow","()Landroid/view/Window;");
    if (!window.isValid()) return;

    // 1) Flag no Window
    const jint FLAG_KEEP_SCREEN_ON =
        QJniObject::getStaticField<jint>("android/view/WindowManager$LayoutParams",
                                         "FLAG_KEEP_SCREEN_ON");
    if (on)
        window.callMethod<void>("addFlags","(I)V", FLAG_KEEP_SCREEN_ON);
    else
        window.callMethod<void>("clearFlags","(I)V", FLAG_KEEP_SCREEN_ON);

    // 2) Sinalizar também no View raiz (alguns vendors só respeitam aqui)
    QJniObject decor = window.callObjectMethod("getDecorView","()Landroid/view/View;");
    if (decor.isValid())
        decor.callMethod<void>("setKeepScreenOn","(Z)V", on);
#else
    Q_UNUSED(on);
#endif
}


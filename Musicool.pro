QT       += core gui widgets multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    androidutils.cpp \
    main.cpp \
    mainwindow.cpp \
    metronomewidget.cpp \
    pitchtracker.cpp \
    staffnotewidget.cpp \
    tonegenerator.cpp \
    tunerwidget.cpp

HEADERS += \
    androidutils.h \
    mainwindow.h \
    metronomewidget.h \
    pitchtracker.h \
    staffnotewidget.h \
    tonegenerator.h \
    tunerwidget.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    MusicoolDir.pro/AndroidManifest.xml \
    MusicoolDir.pro/build.gradle \
    MusicoolDir.pro/res/values/libs.xml \
    MusicoolDir.pro/res/xml/qtprovider_paths.xml \
    android/AndroidManifest.xml \
    android/build.gradle \
    android/res/values/libs.xml \
    android/res/xml/qtprovider_paths.xml \
    android/src/org/qtproject/example/EdgeToEdgeHelper.java

contains(ANDROID_TARGET_ARCH,arm64-v8a) {
    ANDROID_PACKAGE_SOURCE_DIR = \
        $$PWD/MusicoolDir.pro
}

RESOURCES += \
    stuff.qrc

# Só 64-bit (Play exige 64-bit)
ANDROID_ABIS = arm64-v8a
VERSION = 1.0.0           # vira versionName
ANDROID_VERSION_CODE = 1  # inteiro crescente a cada release

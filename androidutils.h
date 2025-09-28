// androidutils.h
#pragma once
#include <QObject>

class AndroidUtils : public QObject {
    Q_OBJECT
public:
    static void applyImmersive(bool on);
    static void keepScreenOn(bool on);
};

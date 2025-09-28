#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "tunerwidget.h"
#include "pitchtracker.h"
#include "metronomewidget.h"
#include <QMainWindow>
#include <QPermission>
#include <QTimer>
#include <QButtonGroup>


#ifdef Q_OS_ANDROID
#include <QPermission>
#endif

#define PAGEINFO  0
#define TUNER     1
#define GENFREQ   2
#define METRONOME 3

constexpr int MIN_BPM = 30;
constexpr int MAX_BPM = 300;

class PitchTracker;
class TunerWidget;
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private slots:
    void onMicrophonePermissionChanged(const QPermission &perm);
    void setBPMvalue(QAbstractButton* button);

private:
    Ui::MainWindow *ui;
    PitchTracker*  m_tracker = nullptr;
    TunerWidget*  m_tuner = nullptr;

    void setupTunerInFrame();
    void startTunerWithPermission();
    void setupToolBoxBehavior();       // start/stop ao trocar de aba
    void wireTunerSignals();

    QButtonGroup *m_group = nullptr; // measure
    QButtonGroup *b_group = nullptr; // bpm

    MetronomeWidget *metro = nullptr;

protected:
    bool event(QEvent *e) override;


};
#endif // MAINWINDOW_H

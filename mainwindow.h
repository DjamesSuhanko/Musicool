#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "tunerwidget.h"
#include "pitchtracker.h"
#include "metronomewidget.h"
#include "tonegenerator.h"
#include <QMainWindow>
#include <QPermission>
#include <QTimer>
#include <QButtonGroup>

//TODO: teste
#include "staffnotewidget.h"

#ifdef Q_OS_ANDROID
#include <QPermission>
#endif

#define PAGEINFO  0
#define TUNER     1
#define GENFREQ   2
#define METRONOME 3

#define OCTAVEMIN 2
#define OCTAVEMAX 5
#define NOTE_DO   0
#define NOTE_SI   6

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
    void emitNote(); //conectar com os botoes de nota; identificar o botão e somar/subtrair, considerando >0 e <7
    void emitOctave(); // TODO: tratar N oitavas

private:
    Ui::MainWindow *ui;

    PitchTracker  *m_tracker = nullptr;
    TunerWidget  *m_tuner    = nullptr;
    MetronomeWidget *metro   = nullptr;
    ToneGenerator *toneGen   = nullptr;
    StaffNoteWidget *staff   = nullptr;

    QButtonGroup *m_group    = nullptr; // measure
    QButtonGroup *b_group    = nullptr; // bpm

    void setupTunerInFrame();
    void startTunerWithPermission();
    void setupToolBoxBehavior();       // start/stop ao trocar de aba
    void wireTunerSignals();
    void setupStaffInFrame();
    void reloadInsetsNow();

    int noteIdxValue = 0;
    int octaveValue  = 4;

signals:
    void noteIdx(int v);
    void octaveIdx(int v);



protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;


};
#endif // MAINWINDOW_H

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
#include "staffnotewidget.h"
#include "compasscalculator.h"
#include "ModernDial.h"

#ifdef Q_OS_ANDROID
#include <QPermission>
#endif

#define PAGEINFO  0
#define TUNER     1
#define FREQUENCY 2
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
    void emitOctave();
    void onPitchDialChanged(int v);
    void onMetronomeVolumeDialChanged(int v);

private:
    Ui::MainWindow *ui;

    PitchTracker  *m_tracker      = nullptr;
    TunerWidget  *m_tuner         = nullptr;
    MetronomeWidget *metro        = nullptr;
    ToneGenerator *toneGen        = nullptr;
    StaffNoteWidget *staff        = nullptr;
    StaffNoteWidget* m_staffTuner = nullptr;

    QButtonGroup *m_group    = nullptr; // measure
    QButtonGroup *b_group    = nullptr; // bpm
    QButtonGroup *calc_group = nullptr; //calculadora

    void setupTunerInFrame();
    void startTunerWithPermission();
    void setupToolBoxBehavior();       // start/stop ao trocar de aba
    void wireTunerSignals();
    void setupStaffInFrame();
    void reloadInsetsNow();
    void setupStaffInTuner();
    void onToolBoxIndexChanged(int idx);

    void setupStackNavigation();      // conecta botões e sinal do stack
    void updateStackTitle() const;    // joga o título atual no lineEdit
    QString currentPageTitle() const; // busca título da página atual

    double mapExp01ToHz(double t01) const;
    double norm01InThisTurn(int v) const;

    int noteIdxValue = 0;
    int octaveValue  = 4;

    QPixmap m_logo;

    // estado para rastrear voltas do dial
    int   m_prevDialValue = 0;
    int   m_turnCounter   = 0;  // pode ser 0..m_turns
    int   m_turns         = 10; // deve coincidir com ModernDial::turns
    int   m_minv          = 0;
    int   m_maxv          = 999; // bom ter bastante resolução por volta

    // faixa de frequência desejada (pode ajustar)
    static constexpr double kMinHz = MetronomeWidget::kMinBeepHz;  // 200
    static constexpr double kMaxHz = MetronomeWidget::kMaxBeepHz;  // 4000


    // ---------------------------
    // estado do dial de VOLUME (separado do pitch!)
    int m_prevVolDialValue = 0;
    int m_volTurnCounter   = 0;

    // (pode reaproveitar m_turns/m_minv/m_maxv se o dial for idêntico,
    //  mas é mais seguro separar também se você quiser ranges diferentes)
    int m_volTurns         = 10;
    int m_volMinv          = 0;
    int m_volMaxv          = 999;




signals:
    void noteIdx(int v);
    void octaveIdx(int v);



protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;


};
#endif // MAINWINDOW_H

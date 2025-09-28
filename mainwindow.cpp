#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "TunerWidget.h"
#include "PitchTracker.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QToolBox>
#include <QFrame>
#include <QGuiApplication>
#include <QDebug>

#include "androidutils.h"
#include <QEvent>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#ifdef Q_OS_ANDROID
static void keepScreenOn(bool on) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    const jint FLAG_KEEP_SCREEN_ON = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
    if (on)  window.callMethod<void>("addFlags",   "(I)V", FLAG_KEEP_SCREEN_ON);
    else     window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
}
#endif

bool MainWindow::event(QEvent *e) {
    if (e->type() == QEvent::WindowActivate) {
        AndroidUtils::applyImmersive(true);
        AndroidUtils::keepScreenOn(true);   // <- reassert
    }
    return QMainWindow::event(e);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->metro = new MetronomeWidget(this);
    metro->setBeatsPerMeasure(4);
    metro->setBpm(ui->lineEdit_metronome->text().toInt());
    metro->setAudioEnabled(true);
    metro->setAccentEnabled(true);
    // adicionar no frame
    auto *lay = qobject_cast<QVBoxLayout*>(ui->frameMetro->layout());
    if (!lay) { lay = new QVBoxLayout(ui->frameMetro); lay->setContentsMargins(0,0,0,0); }
    lay->addWidget(metro);

    m_group = new QButtonGroup(this);
    b_group = new QButtonGroup(this);

    //MEASURE GROUP ====================================
    m_group->addButton(ui->pushButton_2);
    m_group->addButton(ui->pushButton_3);
    m_group->addButton(ui->pushButton_4);

    m_group->setId(ui->pushButton_2,2);
    m_group->setId(ui->pushButton_3,3);
    m_group->setId(ui->pushButton_4,4);

    m_group->setExclusive(true);

    ui->pushButton_2->setCheckable(true);
    ui->pushButton_3->setCheckable(true);
    ui->pushButton_4->setCheckable(true);

    ui->pushButton_4->setChecked(true);

    connect(m_group, &QButtonGroup::idClicked, this, [this](int beats){
        metro->setBeatsPerMeasure(beats);
    });

    //BPM GROUP ========================================
    b_group->addButton(ui->pushButton_less_one);
    b_group->addButton(ui->pushButton_less_10);
    b_group->addButton(ui->pushButton_plus_one);
    b_group->addButton(ui->pushButton_plus_ten);

    b_group->setId(ui->pushButton_less_one,-1);
    b_group->setId(ui->pushButton_less_10,-10);
    b_group->setId(ui->pushButton_plus_one,1);
    b_group->setId(ui->pushButton_plus_ten,10);


    connect(ui->pushButton_start, &QPushButton::clicked, metro, &MetronomeWidget::start);
    connect(ui->pushButton_stop,  &QPushButton::clicked, metro, &MetronomeWidget::stop);


#ifdef Q_OS_ANDROID
    keepScreenOn(true);
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [](Qt::ApplicationState st){ if (st == Qt::ApplicationActive) keepScreenOn(true); });
#endif

     ui->toolBox->setCurrentIndex(PAGEINFO);

    m_tuner   = new TunerWidget(this);
    m_tracker = new PitchTracker(this);

    setupTunerInFrame();
    wireTunerSignals();
    setupToolBoxBehavior();

    // 1) Ao abrir a janela: se a aba atual for TUNER, tenta iniciar
    QTimer::singleShot(0, this, [this]{
        if (ui->toolBox->currentIndex() == TUNER)
            startTunerWithPermission();
    });

    // 2) Robustez: após diálogos e ao voltar ao app (1ª execução, etc.)
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState st){
                if (st == Qt::ApplicationActive && ui->toolBox->currentIndex() == TUNER)
                    startTunerWithPermission();
            });

    //incremento/decremento do BPM
    connect(b_group, &QButtonGroup::buttonClicked, this, &MainWindow::setBPMvalue);

    //desligar metronomo se trocar de aba
    connect(ui->toolBox, &QToolBox::currentChanged, this, [this](int idx){
        if (ui->toolBox->currentIndex() != METRONOME && metro) metro->stop();
    });

    ui->lineEdit_metronome->setReadOnly(true);

}

void MainWindow::setBPMvalue(QAbstractButton* button)
{
    const int delta = b_group->id(button);

    constexpr int MIN_BPM = 30;
    constexpr int MAX_BPM = 300;

    int bpm = std::clamp(metro->bpm() + delta, MIN_BPM, MAX_BPM);
    metro->setBpm(bpm);
    ui->lineEdit_metronome->setText(QString::number(bpm));
}

MainWindow::~MainWindow()
{
    if (m_tracker) m_tracker->stop();
    delete ui;
}

void MainWindow::setupTunerInFrame()
{
    // Coloca o TunerWidget no QFrame (objectName: frameTuner)
    QFrame* frame = ui->frameTuner;
    if (!frame || !m_tuner) return;

    auto *lay = qobject_cast<QVBoxLayout*>(frame->layout());
    if (!lay) {
        lay = new QVBoxLayout();
        lay->setContentsMargins(0,0,0,0);
        lay->setSpacing(0);
        frame->setLayout(lay);
    }

    if (m_tuner->parentWidget() != frame) {
        lay->addWidget(m_tuner);
        m_tuner->show();
    }
}

void MainWindow::wireTunerSignals()
{
    // PitchTracker -> TunerWidget
    connect(m_tracker, &PitchTracker::noteUpdate,
            this, [this](int midi, double cents, double hz, double conf){
                Q_UNUSED(conf);
                if (!m_tuner) return;
                m_tuner->setBaseMidi(midi);
                m_tuner->setCents(cents);
                // qDebug().nospace() << "[Pitch] Hz=" << hz << " midi=" << midi << " cents=" << cents;
            });

    // logs opcionais
    connect(m_tracker, &PitchTracker::started, this, []{
        qDebug() << "[Tracker] started";
    });
    connect(m_tracker, &PitchTracker::stopped, this, []{
        qDebug() << "[Tracker] stopped";
    });

    // parâmetros típicos para celular (ajuste se quiser)
    m_tracker->setMinFrequency(40.0);
    m_tracker->setMaxFrequency(1600.0);
    m_tracker->setAnalysisSize(4096);
    m_tracker->setProcessIntervalMs(35);
    m_tracker->setSilenceRmsThreshold(0.003);
}

void MainWindow::setupToolBoxBehavior()
{
    // Start/stop conforme a aba do QToolBox
    connect(ui->toolBox, &QToolBox::currentChanged, this, [this](int idx){
        if (idx == TUNER) startTunerWithPermission();
        else              m_tracker->stop();
    });
}

void MainWindow::startTunerWithPermission()
{
#ifdef Q_OS_ANDROID
    QMicrophonePermission micPerm;  // vem de <QPermissions>
    switch (qApp->checkPermission(micPerm)) {
    case Qt::PermissionStatus::Granted:
        m_tracker->start();         // idempotente (start pode checar se já está rodando)
        break;
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(micPerm, this,
                                &MainWindow::onMicrophonePermissionChanged);
        break;
    case Qt::PermissionStatus::Denied:
        qDebug() << "[MicPerm] denied";
        break;
    }
#else
    m_tracker->start();
#endif
}

#ifdef Q_OS_ANDROID
void MainWindow::onMicrophonePermissionChanged(const QPermission &perm)
{
    if (perm.status() == Qt::PermissionStatus::Granted)
        startTunerWithPermission();   // passa novamente pelo funil
    else
        qDebug() << "[MicPerm] denied (callback)";
}
#endif

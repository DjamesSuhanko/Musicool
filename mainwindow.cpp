#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "TunerWidget.h"     // ajuste o case conforme o nome no disco
#include "PitchTracker.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QToolBox>
#include <QFrame>
#include <QGuiApplication>
#include <QDebug>

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_ANDROID
    keepScreenOn(true);
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [](Qt::ApplicationState st){ if (st == Qt::ApplicationActive) keepScreenOn(true); });
#endif

    // Se quiser garantir por código que a aba do afinador é a TUNER:
    // ui->toolBox->setCurrentIndex(TUNER);

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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "tunerwidget.h"
#include "pitchtracker.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QToolBox>
#include <QFrame>
#include <QGuiApplication>
#include <QDebug>

#include "androidutils.h"
#include <QEvent>

#include <QStyle>               // necessário para QStyle e seus enums (SP_ArrowRight etc.)
#include <QStyleOption>         // base (se usar opções de estilo)
#include <QStyleOptionSlider>   // se usar QStyleOptionSlider
#include <QProxyStyle>          // se estiver usando QProxyStyle

#include <QScroller>

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

    //REF:ABOUT

    QScroller::grabGesture(ui->textBrowser->viewport(), QScroller::TouchGesture);
    ui->textBrowser->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->textBrowser->setStyleSheet(
        "QScrollBar:vertical{width:16px;margin:0px;}"
        "QScrollBar::handle:vertical{min-height:24px;border-radius:8px;background:#888;}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical{height:0;}"
        );

    ui->textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->textBrowser->setReadOnly(true);
    ui->textBrowser->setOpenExternalLinks(true);
    ui->textBrowser->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->textBrowser->setHtml(R"(
<h2>About the Musicool</h2>
<p align='justify'>Esse aplicativo foi desenvolvido para ser usado
por músicos da CCB, por isso é um aplicativo sem
custo e em constante evolução.</p>

<p>Se você é músico mas não é da CCB, também é gratuito
para você. Apenas diga 1 vez em voz alta:<br>
<b>'Deus seja louvado: Amém!'</b>.</p>

<h2>Tuner</h2>
<p>O Tuner tem o propósito de afinar instrumentos de sopro.
Deve funcionar também com violino, viola e chello.
Ao clicar em <b>Tuner</b>, o microfone precisará ser
aberto pelo aplicativo para 'escutar' seu instrumento.
Ao sair da aba Tuner, o microfone será desligado automaticamente.</p>

<h2>Notes Sound</h2>
<p>Esse é um gerador de frequência, para afinar em qualquer nota desejada.
É possível também usar bemol e sustenido, trocar de nota ou de oitava,
através dos botões.<br>
Play e Stop levam até 2 segundos para iniciar.</p>

<h2>Metronome</h2>
<p>O metrônomo tem seleção de compasso binário, ternário e quaternário.
O ajuste de BPM permite adicionar 1 unidade de tempo ou 10 unidades de tempo por vez.</p>

<h2>Sobre o autor</h2>
<p>Esse aplicativo é uma iniciativa pessoal de <i>Djames Suhanko</i>, não havendo
nenhum vínculo do app com a CCB.</p>
<p>O aplicativo, atualização, segurança e mantenimento é de inteira
responsabilidade do autor.</p>
<br><br>
<p>Que a Paz de Deus esteja em vossos lares. (Amém.)</p>
)");

    //REF:METRONOMO
    ui->lineEdit_metronome->setReadOnly(true);
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


    //REF:TUNER
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

    //=========================== REF:TONE ======================================================
    this->toneGen = new ToneGenerator(this);

    //------------------------------ octave up/down -------------------------------------
    ui->pushButton_octave_down->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    ui->pushButton_octave_down->setIconSize(QSize(32, 32));
    ui->pushButton_octave_down->setProperty("moreOrLess",-1);
    connect(ui->pushButton_octave_down, &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveDown);

    ui->pushButton_octave_up->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    ui->pushButton_octave_up->setIconSize(QSize(32, 32));
    ui->pushButton_octave_up->setProperty("moreOrLess",1);
    connect(ui->pushButton_octave_up,   &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveUp);
    //------------------------------ octave up/down -------------------------------------

    //------------------------------ note up/down ---------------------------------------
    ui->pushButton_previous->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    ui->pushButton_previous->setIconSize(QSize(32, 32));
    ui->pushButton_previous->setProperty("moreOrLess",-1);
    connect(ui->pushButton_previous, SIGNAL(clicked(bool)),this, SLOT(emitNote()));

    ui->pushButton_next->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    ui->pushButton_next->setIconSize(QSize(32, 32));
    ui->pushButton_next->setProperty("moreOrLess",1);
    connect(ui->pushButton_next, SIGNAL(clicked(bool)),this, SLOT(emitNote()));
    //------------------------------ note up/down ---------------------------------------

    //------------------------------ stop/start -----------------------------------------
    ui->pushButton_stop_2->setText(QString::fromUtf8("⏹︎"));
    connect(ui->pushButton_stop_2, &QPushButton::clicked, toneGen, &ToneGenerator::stop);

    ui->pushButton_start_2->setText(QString::fromUtf8("▶"));
    connect(ui->pushButton_start_2, &QPushButton::clicked, toneGen, &ToneGenerator::start);
    //------------------------------ stop/start -----------------------------------------

    //-----------------------------sustenido/bemol---------------------------------------
    ui->pushButton_sharp->setCheckable(true);
    ui->pushButton_bemol->setCheckable(true);

    ui->pushButton_sharp->setText(QString::fromUtf8("♯"));
    ui->pushButton_sharp->setProperty("moreOrLess",1);

    ui->pushButton_bemol->setText(QString::fromUtf8("♭"));
    ui->pushButton_bemol->setProperty("moreOrLess",-1);

    connect(ui->pushButton_sharp, &QPushButton::toggled, this, [=](bool on){
    if (on) ui->pushButton_bemol->setChecked(false);
    toneGen->setAccidental(on ? ToneGenerator::Sharp
                              : (ui->pushButton_bemol->isChecked() ? ToneGenerator::Flat
                                                                   : ToneGenerator::Natural));
});
    connect(ui->pushButton_bemol, &QPushButton::toggled, this, [=](bool on){
    if (on) ui->pushButton_sharp->setChecked(false);
    toneGen->setAccidental(on ? ToneGenerator::Flat
                              : (ui->pushButton_sharp->isChecked() ? ToneGenerator::Sharp
                                                                   : ToneGenerator::Natural));
});

    //-----------------------------sustenido/bemol---------------------------------------


    //-------------------------------- labels -------------------------------------------
    //NOTA
    connect(toneGen, &ToneGenerator::noteLabelChanged, ui->lineEdit_notes, &QLineEdit::setText);

    //FREQ
    connect(toneGen, &ToneGenerator::frequencyChanged, this, [=](double hz){
        ui->lineEdit_freq->setText(QString::number(hz, 'f', 2) + " Hz");
    });

    ui->lineEdit_notes->setReadOnly(true);
    ui->lineEdit_freq->setReadOnly(true);

    //-------------------------------- labels -------------------------------------------

    //connect(this,SIGNAL(noteIdx(int)),this->toneGen,SLOT(setNoteIndex(int)));
    connect(this, &MainWindow::noteIdx, this->toneGen, &ToneGenerator::setNoteIndex);

    toneGen->setNoteIndex(toneGen->noteIndex());  // dispara noteLabelChanged("C4")
    toneGen->setVolume(1.0f);

    connect(toneGen, &ToneGenerator::frequencyChanged, this, [](double hz){
        qDebug() << "[Tone] hz =" << hz;
    });
    connect(ui->pushButton_start_2, &QPushButton::clicked, []{
        qDebug() << "[Tone] START click";
    });


    //-------------- REF:STAFF ---------------------------------------------------------------
    this->staff = new StaffNoteWidget(this);
    staff->setPreferAccidentals(StaffNoteWidget::AccPref::Sharps); // ou Flats/Auto
    staff->setShowLabel(true);

    connect(toneGen, &ToneGenerator::frequencyChanged, staff,  &StaffNoteWidget::setFrequencyHz);


    connect(toneGen, &ToneGenerator::frequencyChanged,
            this, [this](double hz){
                if (staff)
                    staff->setFrequency(hz, StaffNoteWidget::AccPref::Sharps);
            });


    this->staff->setColors(QColor("#121212"), QColor("#3C3C40"), QColor("#FAFAFA"), QColor("#4F8AFF"), QColor("#E0E0E0"));
    this->setupStaffInFrame();

    //=========================== REF:TONE ======================================================

    //REF:ABOUT
    ui->toolBox->setCurrentIndex(PAGEINFO);
}

void MainWindow::setupStaffInFrame()
{
    QFrame* frame = ui->frame_staff;
    if (!frame) return;

    if (!staff)
        staff = new StaffNoteWidget(this);  // cria o widget

    // layout do frame (cria se não existir)
    auto *lay = qobject_cast<QVBoxLayout*>(frame->layout());
    if (!lay) {
        lay = new QVBoxLayout(frame);
        lay->setContentsMargins(0,0,0,0);
        lay->setSpacing(0);
        frame->setLayout(lay);
    }

    // evita adicionar duas vezes
    if (staff->parentWidget() != frame) {
        staff->setClefImageFile(":/sol.png");
        staff->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        lay->addWidget(staff);
        staff->show();
    }

    // (opcional) estilo/cores para combinar com o app
    staff->setColors(QColor("#121212"), QColor("#3C3C40"), QColor("#FAFAFA"), QColor("#4F8AFF"), QColor("#E0E0E0"));
}


void MainWindow::emitOctave()
{
    auto* btn = qobject_cast<QAbstractButton*>(sender());
    if (!btn) return;

    int tmpValue = btn->property("moreOrLess").toInt()+octaveValue;
    if (tmpValue < OCTAVEMIN || tmpValue > OCTAVEMAX){
        return;
    }

    this->octaveValue = tmpValue;
    emit octaveIdx(this->octaveValue);
}

void MainWindow::emitNote()
{
    auto* btn = qobject_cast<QAbstractButton*>(sender());
    if (!btn) return;

    int tmpValue = btn->property("moreOrLess").toInt()+noteIdxValue;
    if (tmpValue < NOTE_DO || tmpValue > NOTE_SI){
        return;
    }

    this->noteIdxValue = tmpValue;
    emit noteIdx(this->noteIdxValue);

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

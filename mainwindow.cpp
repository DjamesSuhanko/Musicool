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
#include <QEvent>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionSlider>
#include <QProxyStyle>
#include <QScroller>
#include <theme.h>

#ifdef Q_OS_ANDROID
#include <QtCore/qjniobject.h>
#include <QtCore/qnativeinterface.h>

static inline void keepScreenOnQt6(bool on = true)
{
    // Activity do Qt
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    // Window da Activity
    QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    const jint FLAG_KEEP_SCREEN_ON = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON

    if (on) {
        window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    } else {
        window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    }

    // (opcional, ajuda em alguns aparelhos)
    QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
    if (decor.isValid()) {
        decor.callMethod<void>("setKeepScreenOn", "(Z)V", jboolean(on));
    }
}
#endif



#ifdef Q_OS_ANDROID
#include <QtCore/qnativeinterface.h>
#include <QtCore/QJniObject>

static inline QJniObject qtActivity() {
    return QNativeInterface::QAndroidApplication::context();
}

static inline void jniRequestApplyInsets() {
    if (auto act = qtActivity(); act.isValid()) {
        QJniObject::callStaticMethod<void>(
            "org/qtproject/example/EdgeToEdgeHelper",
            "requestApplyInsets",
            "(Landroid/app/Activity;)V",
            act.object<jobject>());
    }
}

static inline int jniBottomInsetPx() {
    auto act = qtActivity();
    if (!act.isValid()) return 0;
    QJniObject arr = QJniObject::callStaticObjectMethod(
        "org/qtproject/example/EdgeToEdgeHelper",
        "getSystemBarInsets",
        "(Landroid/app/Activity;)[I",
        act.object<jobject>());
    if (!arr.isValid()) return 0;
    return QJniObject::callStaticMethod<jint>(
        "java/lang/reflect/Array","getInt","(Ljava/lang/Object;I)I",
        arr.object<jobject>(), 3); // bottom
}
#endif

static void applyBottomInsetToFooterSpacer(QWidget* central) {
#ifndef Q_OS_ANDROID
    Q_UNUSED(central);
#else
    if (!central) return;

    // Evita margem dupla no grid raiz
    if (auto *grid = central->findChild<QGridLayout*>("gridLayout_3")) {
        grid->setContentsMargins(10,40,10,40);
        grid->setSpacing(0);
    }

    // No seu .ui, o spacer do rodapé é o ÚLTIMO item do verticalLayout_TUDO
    if (auto *vTudo = central->findChild<QVBoxLayout*>("verticalLayout_TUDO")) {
        if (vTudo->count() == 0) return;
        if (QLayoutItem *last = vTudo->itemAt(vTudo->count()-1); last && last->spacerItem()) {
            last->spacerItem()->changeSize(0, qMax(0, jniBottomInsetPx()),
                                           QSizePolicy::Preferred, QSizePolicy::Fixed);
            vTudo->invalidate();
        }
    }
#endif
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
    switch (ev->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        reloadInsetsNow();
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(obj, ev); // não consome, só reage
}

void MainWindow::reloadInsetsNow() {
#ifdef Q_OS_ANDROID
    // 1) pede novo dispatch de insets ao Android
    jniRequestApplyInsets();

    // 2) aplica imediatamente o inset atual ao spacer
    applyBottomInsetToFooterSpacer(centralWidget());

    // 3) reforço no próximo ciclo (caso o dispatch chegue um “tick” depois)
    QTimer::singleShot(0, this, [this]{
        applyBottomInsetToFooterSpacer(centralWidget());
    });
#endif
}

void MainWindow::setupStaffInTuner()
{
    QFrame* f = ui->frameStaffTuner;
    if (!f) return;

    if (!m_staffTuner) {
        m_staffTuner = new StaffNoteWidget(this);
        m_staffTuner->setPreferAccidentals(StaffNoteWidget::AccPref::Sharps); // ou Flats/Auto
        m_staffTuner->setClefImageFile(":/sol.png");
        // opcional: combinar com tema
        // m_staffTuner->setColors(QColor("#121212"), QColor("#3C3C40"),
        //                         QColor("#FAFAFA"), QColor("#4F8AFF"), QColor("#E0E0E0"));
    }

    auto *lay = qobject_cast<QVBoxLayout*>(f->layout());
    if (!lay) { lay = new QVBoxLayout(f); lay->setContentsMargins(0,0,0,0); lay->setSpacing(0); }
    if (m_staffTuner->parentWidget() != f) lay->addWidget(m_staffTuner);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //-- forçar cor clara dos textos - START -----
    qApp->setStyleSheet(R"(
  QWidget { background: #121212; }
  * { color: #EEEEEE; } /* texto claro por padrão */
  QLineEdit, QTextEdit, QTextBrowser, QPlainTextEdit {
    background: #1A1B1E;
    selection-background-color: #4F8AFF;
    selection-color: #FFFFFF;
  }
  QToolTip { color: #121212; background: #EEEEEE; })");

    ui->textBrowser->document()->setDefaultStyleSheet("body{color:#EEEEEE;}");

    //-- forçar cor clara dos textos - END ------


    applyBottomInsetToFooterSpacer(ui->centralwidget);
    keepScreenOnQt6(true);

    qApp->installEventFilter(this);                  // captura toques em toda a app
    centralWidget()->setAttribute(Qt::WA_AcceptTouchEvents, true); // garante eventos de toque


    ui->labelMusicool->setAlignment(Qt::AlignCenter);
    ui->labelMusicool->setMaximumHeight(128);
    ui->labelMusicool->setMaximumWidth(256);
    ui->labelMusicool->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QPixmap px(":/imgs/MusicoolCapa.png");
    //ui->labelMusicool->setPixmap(px.scaled(ui->labelMusicool->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->labelMusicool->setPixmap(px);
    ui->labelMusicool->setScaledContents(true);
    ui->verticalLayout_10->setAlignment(ui->labelMusicool, Qt::AlignHCenter);
    ui->labelMusicool->setStyleSheet(
        "#labelMusicool {"
        "  border: 2px solid #3C3C40;"
        "  border-radius: 16px;"
        "  background-color: #1e1f22;"
        "}"
        );

    ui->lineEdit_metronome->setObjectName("lineEdit_metronome");
    ui->lineEdit_metronome->setStyleSheet(
        "#lineEdit_metronome {"
        "  border-image: url(:/imgs/MusicoolCapaLineEdit.png) 0 0 0 0 stretch stretch;"
        "  border: 1px solid #3C3C40;"   /* opcional: borda sobreposta */
        "  border-radius: 12px;"
        "  color: #eeeeee;"
        "  padding: 6px;"
        "}"
        );

    // ===== ABOUT =====
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
<h2>Sobre o Musicool</h2>
<p align='justify'>Esse aplicativo foi desenvolvido para ser usado
por músicos da CCB, por isso é um aplicativo sem
custo e em constante evolução.</p>

<p>Se você é músico mas não é da CCB, também é gratuito
para você. Apenas diga 1 vez em voz alta:<br>
<b>'Deus seja louvado: Amém!'</b>.</p>

<h2>Tuner</h2>
<p>O Tuner tem o propósito de afinar instrumentos de sopro.
Deve funcionar também com violino, viola e celo.
Ao clicar em <b>Tuner</b>, o microfone precisará ser
aberto pelo aplicativo para 'escutar' seu instrumento.
Ao sair da aba Tuner, o microfone será desligado automaticamente.</p>

<h2>Frequency</h2>
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

    // ===== REF:METRONOME =====
    ui->lineEdit_metronome->setReadOnly(true);
    this->metro = new MetronomeWidget(this);
    metro->setBeatsPerMeasure(4);
    metro->setBpm(ui->lineEdit_metronome->text().toInt());
    metro->setAudioEnabled(true);
    metro->setAccentEnabled(true);

    if (auto *lay = qobject_cast<QVBoxLayout*>(ui->frameMetro->layout())) {
        lay->addWidget(metro);
    } else {
        auto *lay2 = new QVBoxLayout(ui->frameMetro);
        lay2->setContentsMargins(0,0,0,0);
        lay2->addWidget(metro);
    }

    m_group = new QButtonGroup(this);
    b_group = new QButtonGroup(this);

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

    b_group->addButton(ui->pushButton_less_one);
    b_group->addButton(ui->pushButton_less_10);
    b_group->addButton(ui->pushButton_plus_one);
    b_group->addButton(ui->pushButton_plus_ten);

    b_group->setId(ui->pushButton_less_one,-1);
    b_group->setId(ui->pushButton_less_10,-10);
    b_group->setId(ui->pushButton_plus_one,1);
    b_group->setId(ui->pushButton_plus_ten,10);

    connect(b_group,
            qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked),
            this, &MainWindow::setBPMvalue);


    connect(ui->pushButton_start, &QPushButton::clicked, metro, &MetronomeWidget::start);
    connect(ui->pushButton_stop,  &QPushButton::clicked, metro, &MetronomeWidget::stop);

    // ===== TUNER =====
    m_tuner   = new TunerWidget(this);
    m_tracker = new PitchTracker(this);

    setupTunerInFrame();
    wireTunerSignals();
    setupToolBoxBehavior();

    QTimer::singleShot(0, this, [this]{
        if (ui->toolBox->currentIndex() == TUNER)
            startTunerWithPermission();
    });

    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState st){
                if (st == Qt::ApplicationActive && ui->toolBox->currentIndex() == TUNER)
                    startTunerWithPermission();
            });

    // ===== REF:NOTES SOUND =====
    this->toneGen = new ToneGenerator(this);

    ui->pushButton_octave_down->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    ui->pushButton_octave_down->setIconSize(QSize(32, 32));
    ui->pushButton_octave_down->setProperty("moreOrLess",-1);
    connect(ui->pushButton_octave_down, &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveDown);

    ui->pushButton_octave_up->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    ui->pushButton_octave_up->setIconSize(QSize(32, 32));
    ui->pushButton_octave_up->setProperty("moreOrLess",1);
    connect(ui->pushButton_octave_up,   &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveUp);

    ui->pushButton_previous->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    ui->pushButton_previous->setIconSize(QSize(32, 32));
    ui->pushButton_previous->setProperty("moreOrLess",-1);
    connect(ui->pushButton_previous, SIGNAL(clicked(bool)),this, SLOT(emitNote()));

    ui->pushButton_next->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    ui->pushButton_next->setIconSize(QSize(32, 32));
    ui->pushButton_next->setProperty("moreOrLess",1);
    connect(ui->pushButton_next, SIGNAL(clicked(bool)),this, SLOT(emitNote()));

    ui->pushButton_stop_2->setText(QString::fromUtf8("⏹︎"));
    connect(ui->pushButton_stop_2, &QPushButton::clicked, toneGen, &ToneGenerator::stop);

    ui->pushButton_start_2->setText(QString::fromUtf8("▶"));
    connect(ui->pushButton_start_2, &QPushButton::clicked, toneGen, &ToneGenerator::start);

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

    connect(toneGen, &ToneGenerator::noteLabelChanged, ui->lineEdit_notes, &QLineEdit::setText);
    connect(toneGen, &ToneGenerator::frequencyChanged, this, [=](double hz){
        ui->lineEdit_freq->setText(QString::number(hz, 'f', 2) + " Hz");
    });

    ui->lineEdit_notes->setReadOnly(true);
    ui->lineEdit_freq->setReadOnly(true);

    connect(this, &MainWindow::noteIdx, this->toneGen, &ToneGenerator::setNoteIndex);
    toneGen->setNoteIndex(toneGen->noteIndex());
    toneGen->setVolume(1.0f);

    connect(toneGen, &ToneGenerator::frequencyChanged, this, [](double hz){
        qDebug() << "[Tone] hz =" << hz;
    });

    // ===== STAFF =====
    this->staff = new StaffNoteWidget(this);
    staff->setPreferAccidentals(StaffNoteWidget::AccPref::Sharps);
    staff->setShowLabel(true);

    connect(toneGen, &ToneGenerator::frequencyChanged, staff,  &StaffNoteWidget::setFrequencyHz);
    connect(toneGen, &ToneGenerator::frequencyChanged, this, [this](double hz){
        if (staff) staff->setFrequency(hz, StaffNoteWidget::AccPref::Sharps);
    });

    this->staff->setColors(QColor("#121212"), QColor("#3C3C40"),
                           QColor("#FAFAFA"), QColor("#4F8AFF"), QColor("#E0E0E0"));
    this->setupStaffInFrame();

    // ===== DEFAULT TAB =====
    ui->toolBox->setCurrentIndex(PAGEINFO);

    // ======= REF:STAFF TUNER =========
    setupStaffInTuner();

    // Atualiza a pauta quando o tracker detecta tom
    connect(m_tracker, &PitchTracker::noteUpdate,
            this, [this](int midi, double cents, double hz, double conf){
        // filtro simples para evitar “tremidas” em silêncio/baixa confiança
        if (conf < 0.5 || hz <= 0.0) return;

        // 1) por MIDI (mais estável para desenho)
        if (m_staffTuner) m_staffTuner->setMidi(midi, StaffNoteWidget::AccPref::Sharps);

        // 2) (opcional) por Hz — se quiser refletir microvariações
        // if (m_staffTuner) m_staffTuner->setFrequency(hz, StaffNoteWidget::AccPref::Sharps);
    });
}

bool MainWindow::event(QEvent *e)
{
    // if (e->type() == QEvent::ApplicationPaletteChange ||
    //     e->type() == QEvent::StyleChange ||
    //     e->type() == QEvent::ThemeChange) {
    //     QApplication::setPalette(Theme::darkPalette());
    //     qApp->setStyleSheet(Theme::globalQss());
    // }
    return QMainWindow::event(e);
}

void MainWindow::setupStaffInFrame()
{
    QFrame* frame = ui->frame_staff;
    if (!frame) return;

    if (!staff)
        staff = new StaffNoteWidget(this);

    auto *lay = qobject_cast<QVBoxLayout*>(frame->layout());
    if (!lay) {
        lay = new QVBoxLayout(frame);
        lay->setContentsMargins(0,0,0,0);
        lay->setSpacing(0);
        frame->setLayout(lay);
    }

    if (staff->parentWidget() != frame) {
        staff->setClefImageFile(":/sol.png");
        staff->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        lay->addWidget(staff);
        staff->show();
    }

    staff->setColors(QColor("#121212"), QColor("#3C3C40"),
                     QColor("#FAFAFA"), QColor("#4F8AFF"), QColor("#E0E0E0"));
}

void MainWindow::emitOctave()
{
    auto* btn = qobject_cast<QAbstractButton*>(sender());
    if (!btn) return;

    int tmpValue = btn->property("moreOrLess").toInt()+octaveValue;
    if (tmpValue < OCTAVEMIN || tmpValue > OCTAVEMAX) return;

    this->octaveValue = tmpValue;
    emit octaveIdx(this->octaveValue);
}

void MainWindow::emitNote()
{
    auto* btn = qobject_cast<QAbstractButton*>(sender());
    if (!btn) return;

    int tmpValue = btn->property("moreOrLess").toInt()+noteIdxValue;
    if (tmpValue < NOTE_DO || tmpValue > NOTE_SI) return;

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
    connect(m_tracker, &PitchTracker::noteUpdate,
            this, [this](int midi, double cents, double hz, double conf){
                Q_UNUSED(conf);
                if (!m_tuner) return;
                m_tuner->setBaseMidi(midi);
                m_tuner->setCents(cents);
            });

    connect(m_tracker, &PitchTracker::started, this, []{
        qDebug() << "[Tracker] started";
    });
    connect(m_tracker, &PitchTracker::stopped, this, []{
        qDebug() << "[Tracker] stopped";
    });

    m_tracker->setMinFrequency(40.0);
    m_tracker->setMaxFrequency(1600.0);
    m_tracker->setAnalysisSize(4096);
    m_tracker->setProcessIntervalMs(35);
    m_tracker->setSilenceRmsThreshold(0.003);
}

void MainWindow::setupToolBoxBehavior()
{
    // aplica estado já na abertura
    onToolBoxIndexChanged(ui->toolBox->currentIndex());

    // agora sim: UniqueConnection OK, pois é slot membro
    connect(ui->toolBox, &QToolBox::currentChanged,
            this, &MainWindow::onToolBoxIndexChanged,
            Qt::UniqueConnection);
}

void MainWindow::onToolBoxIndexChanged(int idx)
{
    // Tuner
    if (idx == TUNER)  startTunerWithPermission();
    else if (m_tracker) m_tracker->stop();

    // Metronome: pare sempre que a aba ativa NÃO for o metrônomo
    if (idx != METRONOME && metro) metro->stop();
    if (idx != FREQUENCY && toneGen) toneGen->stop();
}


void MainWindow::startTunerWithPermission()
{
#ifdef Q_OS_ANDROID
    QMicrophonePermission micPerm;
    switch (qApp->checkPermission(micPerm)) {
    case Qt::PermissionStatus::Granted:
        m_tracker->start();
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
        startTunerWithPermission();
    else
        qDebug() << "[MicPerm] denied (callback)";
}
#endif

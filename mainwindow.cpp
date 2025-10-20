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
#include <QFontDatabase>

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

    // no ctor:
    int id = QFontDatabase::addApplicationFont(":/Fonts/NotoMusic-Regular.ttf");
    // ou ":/fonts/BravuraText.ttf"
    QString family = QFontDatabase::applicationFontFamilies(id).value(0);
    QFont music(family);
    music.setPointSizeF(ui->lineEdit_calc_notes->font().pointSizeF() * 1.2); // opcional

    ui->lineEdit_calc_notes->setFont(music);

    QFont tbFont = music;
    tbFont.setPointSizeF( tbFont.pointSizeF() * 0.6 );
    ui->toolBox->setFont(tbFont);



    ui->toolBox->setItemText(0, QString::fromUtf8(u8"\U0001D15F") + " Bag");
    ui->toolBox->setItemText(1, QString::fromUtf8(u8"\U0001D15E") + " Tuner");
    ui->toolBox->setItemText(2, QString::fromUtf8(u8"\U0001D15E") + QString::fromUtf8(u8"\U0001D15F") + " Frequency");
    ui->toolBox->setItemText(3, QString::fromUtf8(u8"\U0001D15D") + " Metronome");


    setupStackNavigation();
    updateStackTitle();


    //REF:CALCULATOR
    //semibreve
    ui->pushButton_calc_semibreve->setText("");
    ui->pushButton_calc_semibreve->setIcon(QIcon(QStringLiteral(":/imgs/001.png")));
    ui->pushButton_calc_semibreve->setIconSize(QSize(32, 32));
    //pausa de semibreve
    ui->pushButton_calc_pause_1->setText("");
    ui->pushButton_calc_pause_1->setIcon(QIcon(QStringLiteral(":/imgs/002p.png")));
    ui->pushButton_calc_pause_1->setIconSize(QSize(32, 32));

    //minima
    ui->pushButton_calc_minima->setText("");
    ui->pushButton_calc_minima->setIcon(QIcon(QStringLiteral(":/imgs/002.png")));
    ui->pushButton_calc_minima->setIconSize(QSize(32, 32));
    //pausa de minima
    ui->pushButton_calc_pause_2->setText("");
    ui->pushButton_calc_pause_2->setIcon(QIcon(QStringLiteral(":/imgs/001p.png")));
    ui->pushButton_calc_pause_2->setIconSize(QSize(32, 32));

    //seminima
    ui->pushButton_calc_seminima->setText("");
    ui->pushButton_calc_seminima->setIcon(QIcon(QStringLiteral(":/imgs/003.png")));
    ui->pushButton_calc_seminima->setIconSize(QSize(32, 32));
    //pausa de seminima
    ui->pushButton_calc_pause_4->setText("");
    ui->pushButton_calc_pause_4->setIcon(QIcon(QStringLiteral(":/imgs/003p.png")));
    ui->pushButton_calc_pause_4->setIconSize(QSize(32, 32));


    //colcheia
    ui->pushButton_calc_colcheia->setText("");
    ui->pushButton_calc_colcheia->setIcon(QIcon(QStringLiteral(":/imgs/004.png")));
    ui->pushButton_calc_colcheia->setIconSize(QSize(32, 32));
    //pausa de colcheia
    ui->pushButton_calc_pause_8->setText("");
    ui->pushButton_calc_pause_8->setIcon(QIcon(QStringLiteral(":/imgs/004p.png")));
    ui->pushButton_calc_pause_8->setIconSize(QSize(32, 32));

    //semicolcheia
    ui->pushButton_calc_semicolcheia->setText("");
    ui->pushButton_calc_semicolcheia->setIcon(QIcon(QStringLiteral(":/imgs/005.png")));
    ui->pushButton_calc_semicolcheia->setIconSize(QSize(32, 32));
    //pausa de semicolcheia
    ui->pushButton_calc_pause_16->setText("");
    ui->pushButton_calc_pause_16->setIcon(QIcon(QStringLiteral(":/imgs/005p.png")));
    ui->pushButton_calc_pause_16->setIconSize(QSize(32, 32));

    //backspace
    ui->pushButton_calc_backspace->setText("");
    ui->pushButton_calc_backspace->setIcon(QIcon(QStringLiteral(":/imgs/arrowL.png")));
    ui->pushButton_calc_backspace->setIconSize(QSize(16, 16));

    calc_group = new QButtonGroup(this);

    calc_group->addButton(ui->pushButton_calc_semibreve,      CompassCalculator::BTN_NOTE_SEMIBREVE);
    calc_group->addButton(ui->pushButton_calc_minima,         CompassCalculator::BTN_NOTE_MINIMA);
    calc_group->addButton(ui->pushButton_calc_seminima,       CompassCalculator::BTN_NOTE_SEMINIMA);
    calc_group->addButton(ui->pushButton_calc_colcheia,       CompassCalculator::BTN_NOTE_COLCHEIA);
    calc_group->addButton(ui->pushButton_calc_semicolcheia,   CompassCalculator::BTN_NOTE_SEMICOLCHEIA);

    calc_group->addButton(ui->pushButton_calc_pause_1,       CompassCalculator::BTN_REST_SEMIBREVE);
    calc_group->addButton(ui->pushButton_calc_pause_2,       CompassCalculator::BTN_REST_MINIMA);
    calc_group->addButton(ui->pushButton_calc_pause_4,       CompassCalculator::BTN_REST_SEMINIMA);
    calc_group->addButton(ui->pushButton_calc_pause_8,       CompassCalculator::BTN_REST_COLCHEIA);
    calc_group->addButton(ui->pushButton_calc_pause_16,      CompassCalculator::BTN_REST_SEMICOLCHEIA);

    calc_group->addButton(ui->pushButton_cal_dot,            CompassCalculator::BTN_DOT);
    calc_group->addButton(ui->pushButton_calc_backspace,     CompassCalculator::BTN_BACKSPACE);
    calc_group->addButton(ui->pushButton_calc_clear,         CompassCalculator::BTN_CLEAR);

    //REF:STACK
    ui->pushButton_stackBack->setText("");
    ui->pushButton_stackBack->setIcon(QIcon(":/imgs/arrowL.png"));
    ui->pushButton_stackBack->setIconSize(QSize(16, 16));
    ui->pushButton_stackNext->setText("");
    ui->pushButton_stackNext->setIcon(QIcon(":/imgs/arrowR.png"));
    ui->pushButton_stackNext->setIconSize(QSize(16, 16));

    //WARNING ALERT TODO: sempre deixar a mainwindow.ui na aba da calculadora. Ao iniciar o app, a troca é automática para About
    //e isso resolve o problema de diagramação da janela
    ui->stackWidget->widget(0)->setProperty("title", "About");
    ui->stackWidget->widget(1)->setProperty("title", "Claves");
    ui->stackWidget->widget(2)->setProperty("title", "Figuras Musicais");
    ui->stackWidget->widget(3)->setProperty("title", "Calculadora");

    auto *calc = new CompassCalculator(this);
    calc->setButtonGroup(calc_group);

    // // no ctor:
    // int id = QFontDatabase::addApplicationFont(":/Fonts/NotoMusic-Regular.ttf");
    // // ou ":/fonts/BravuraText.ttf"
    // QString family = QFontDatabase::applicationFontFamilies(id).value(0);
    // QFont music(family);
    // music.setPointSizeF(ui->lineEdit_calc_notes->font().pointSizeF() * 1.2); // opcional

    // ui->lineEdit_calc_notes->setFont(music);
    //ui->labelSeq->setTextFormat(Qt::RichText);

    // connect(calc, &CompassCalculator::totalChanged, this,
    //         [this](const Frac& , const QString& txt){
    //             ui->labelTopCalc->setText(txt); // fração da semibreve
    //         });

    ui->lineEdit_calc_notes->setReadOnly(true);
    ui->lineEdit_calc_notes->setFrame(false);
    ui->lineEdit_calc_notes->setFocusPolicy(Qt::NoFocus);
    ui->lineEdit_calc_notes->setCursor(Qt::ArrowCursor);
    ui->lineEdit_calc_notes->setTextMargins(4, 0, 4, 0);  // um respiro
    ui->lineEdit_calc_notes->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // NÃO deixe crescer verticalmente
    //ui->lineEdit_calc_notes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    //ui->lineEdit_calc_notes->setFixedHeight(ui->lineEdit_notes->sizeHint().height());

    connect(calc, &CompassCalculator::sequenceChanged, this,
            [this](const QString& seq){
                ui->lineEdit_calc_notes->setText(seq);   // "♩ + ♪ + . + 𝄽"
            });

    connect(calc, &CompassCalculator::signatureChanged, this,
            [this](int N, int D, const QString& textND, const QString& html){
                ui->labelSignaturePlain->setText(textND); // "3/4"
                //ui->labelSignatureStacked->setText(html); // empilhado
                //ui->labelSignatureStacked->setTextFormat(Qt::RichText);
            });


    ui->beatSlider->setColors(QColor("#0B3D0B"),  // trilho
                              QColor("#0B3D0B"),  // ativo (mesma cor → nada de azul)
                              QColor("#B0B0B0"),  // ticks
                              QColor("#A8FF00"),  // knob
                              QColor("#E0E0E0")); // labels


    ui->beatSlider->setStyleSheet(
        "QSlider{ background: transparent; }"
        "QSlider::groove:horizontal{"
        "  background:#0B3D0B; height:8px; border-radius:4px;"
        "}"
        "QSlider::sub-page:horizontal{"
        "  background:#0B3D0B; border-radius:4px;"
        "}"
        "QSlider::add-page:horizontal{"
        "  background:transparent;"
        "}"
        "QSlider::handle:horizontal{"
        "  background:#A8FF00; width:24px; height:24px;"
        "  margin:-8px 0; border-radius:12px;"
        "}"
        );

    // ficar “baixinho”
    ui->beatSlider->setCompactPresetSmall();
    // ou ajuste fino:
    ui->beatSlider->setTrackThickness(6);
    ui->beatSlider->setHandleRadius(12);
    ui->beatSlider->setVerticalPadding(4);

    // sem labels, se quiser ainda mais compacto visualmente
    // ui->beatSlider->setShowLabels(false);

    //-- forçar cor clara dos textos - START -----
    qApp->setStyleSheet(R"(
  QWidget { background: #121212; }
  * { color: #EEEEEE; } /* texto claro por padrão */
  QLineEdit, QTextEdit, QTextBrowser, QPlainTextEdit {
    background: #1A1B1E;
    selection-background-color: #0B3D0B;
    selection-color: #FFFFFF;
  }
  QToolTip { color: #121212; background: #EEEEEE; })");

    ui->textBrowser->document()->setDefaultStyleSheet("body{color:#EEEEEE;}");

    //-- forçar cor clara dos textos - END ------


    applyBottomInsetToFooterSpacer(ui->centralwidget);
    keepScreenOnQt6(true);

    qApp->installEventFilter(this);                  // captura toques em toda a app
    centralWidget()->setAttribute(Qt::WA_AcceptTouchEvents, true); // garante eventos de toque


    // ui->labelMusicool->setAlignment(Qt::AlignCenter);
    // ui->labelMusicool->setMaximumHeight(128);
    // ui->labelMusicool->setMaximumWidth(256);
    // ui->labelMusicool->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    // QPixmap px(":/imgs/MusicoolCapa.png");
    // //ui->labelMusicool->setPixmap(px.scaled(ui->labelMusicool->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    // ui->labelMusicool->setPixmap(px);
    // ui->labelMusicool->setScaledContents(true);
    // ui->verticalLayout_10->setAlignment(ui->labelMusicool, Qt::AlignHCenter);
    // ui->labelMusicool->setStyleSheet(
    //     "#labelMusicool {"
    //     "  border: 2px solid #3C3C40;"
    //     "  border-radius: 16px;"
    //     "  background-color: #1e1f22;"
    //     "}"
    //     );

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
    ui->textBrowser->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->textBrowser->setHtml(R"(
<h2>Sobre o Musicool</h2>
<p align='justify'>Esse aplicativo foi desenvolvido para ser usado
por músicos da CCB, por isso é um aplicativo sem
custo e em constante evolução.</p>

<p align='justify'>Se você é músico mas não é da CCB, também é gratuito
para você. Apenas diga 1 vez em voz alta:<br>
<b>'Deus seja louvado: Amém!'</b>.</p>

<div align='center'>
<img src='qrc:/imgs/MusicoolCapa.png' width='200'/>
</div>

<h2>Bag</h2>
<p align='justify'>Nesse menu estarão conceitos musicais utilizados no GEM. Evoluções estão
 previstas, mas não planejadas, portanto, podem surgir ferramentas novas ou inovadoras para
utilizarmos em nossos estudos.</p>
<p align='justify'>Use as setas acima para navegar pela Bag.</p>
<h2>Tuner</h2>
<p align='justify'>O Tuner tem o propósito de afinar instrumentos de sopro.
Deve funcionar também com violino, viola e celo.
Ao clicar em <b>Tuner</b>, o microfone precisará ser
aberto pelo aplicativo para 'escutar' seu instrumento.
Ao sair da aba Tuner, o microfone será desligado automaticamente pelo Android, ao
notar que o microfone não está mais em uso.</p>

<h2>Frequency</h2>
<p align='justify'>Esse é um gerador de frequência, para afinar em qualquer nota desejada.
É possível também usar bemol e sustenido, trocar de nota ou de oitava,
através dos botões.<br>
Play e Stop levam até 2 segundos para iniciar.</p>

<h2>Metronome</h2>
<p align='justify'>O metrônomo tem seleção de compasso binário, ternário e quaternário.
O ajuste de BPM permite adicionar 1 unidade de tempo ou 10 unidades de tempo por vez.</p>

<h2>Sobre o autor</h2>
<p align='justify'>Esse aplicativo é uma iniciativa pessoal de <i>Djames Suhanko</i>, não havendo
nenhum vínculo do app com a CCB.</p>
<p>O aplicativo, atualização, segurança e mantenimento é de inteira
responsabilidade do autor.</p>
<br>
<p>Que a Paz de Deus esteja em vossos lares. (Amém.)</p>
)");

    QScroller::grabGesture(ui->textBrowser_claves->viewport(), QScroller::TouchGesture);
    ui->textBrowser_claves->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->textBrowser_claves->setStyleSheet(
        "QScrollBar:vertical{width:16px;margin:0px;}"
        "QScrollBar::handle:vertical{min-height:24px;border-radius:8px;background:#888;}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical{height:0;}"
        );
    ui->textBrowser_claves->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->textBrowser_claves->setReadOnly(true);
    ui->textBrowser_claves->setOpenExternalLinks(true);
    ui->textBrowser_claves->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->textBrowser_claves->setHtml(R"(

<h2>Endecagrama</h2>
<div align='center'>
<img src='qrc:/imgs/endecagrama.png' width='200'/>
</div>
<p align='justify'>Na ordem, vemos a clave <b>Sol</b>, <b>Dó</b> e <b>Fá</b>.</p>
<p align='justify'>O <b>Dó</b> da região média é o Dó3, que em cifra é o C4. Esses
são dois dos sistemas de numeração de oitavas, que divergem no ponto de contagem inicial
(um sistema começa em 0, o outro em 1), mas atente-se a isso: O 'Dó central' tem 261.63Hz.
 Esse número se refere à frequência de ondas emitidas pelo Dó central.</p>

<p align='justify'>Alinhando o Dó3 das três claves, temos 11 linhas, que formam o
<i>Endecagrama</i>.</p>
<p align='justify'>A clave <b>Sol</b> é a clave dos agudos; a clave <b>Dó</b>, dos médios; e a
 clave <b>Fá</b>, dos graves.</p>
<p align='justify'>Cada clave marca sua respectiva nota de referência. Na clave <b>Sol</b>, a
linha de sol é envolvida pelo círculo da clave. Na clave <b>Dó</b>, o Dó está na linha central,
bem no centro da lira. Na clave de <b>Fá</b>, a linha de Fá está entre os dois pontos. Repare que
na clave de <b>Fá</b> à esquerda, a bolinha da curva também está sobre o Fá. O 'Dó comum' das
claves está dentro do pentagrama, bastando usar a nota de referência para encontrar a posição de Dó.
)");


    const QString html = R"(
<div align="center">
<table border="1" cellpadding="6" cellspacing="0" width="100%">
  <tr bgcolor="#f5f5f5" align="center">
    <th><font color="#000000">FIGURA</font></th>
    <th><font color="#000000">NOME</font></th>
    <th><font color="#000000">PAUSA</font></th>
    <th><font color="#000000">VALOR</font></th>
    <th><font color="#000000">TEMPO</font></th>
  </tr>

  <!-- Semibreve -->
  <tr align="center">
    <td><img src=":/imgs/001.png" alt="Semibreve" width="32" height="32"></td>
    <td align="left">Semibreve</td>
    <td><img src=":/imgs/002p.png" alt="Pausa de semibreve" width="32" height="32"></td>
    <td>1</td>
    <td>4</td>
  </tr>

  <!-- Mínima -->
  <tr align="center">
    <td><img src=":/imgs/002.png" alt="Mínima" width="32" height="32"></td>
    <td align="left">Mínima</td>
    <td><img src=":/imgs/001p.png" alt="Pausa de mínima" width="32" height="32"></td>
    <td>2</td>
    <td>2</td>
  </tr>

  <!-- Semínima -->
  <tr align="center">
    <td><img src=":/imgs/003.png" alt="Semínima" width="32" height="32"></td>
    <td align="left">Semínima</td>
    <td><img src=":/imgs/003p.png" alt="Pausa de semínima" width="32" height="32"></td>
    <td>4</td>
    <td>1</td>
  </tr>

  <!-- Colcheia -->
  <tr align="center">
    <td><img src=":/imgs/004.png" alt="Colcheia" width="32" height="32"></td>
    <td align="left">Colcheia</td>
    <td><img src=":/imgs/004p.png" alt="Pausa de colcheia" width="32" height="32"></td>
    <td>8</td>
    <td>1/2</td>
  </tr>

  <!-- Semicolcheia -->
  <tr align="center">
    <td><img src=":/imgs/005.png" alt="Semicolcheia" width="32" height="32"></td>
    <td align="left">Semicolcheia</td>
    <td><img src=":/imgs/005p.png" alt="Pausa de semicolcheia" width="32" height="32"></td>
    <td>16</td>
    <td>1/4</td>
  </tr>
</table>
</div>

<p align='justify'>O valor de referência é a proporção de notas que cabe na Semibreve. É fácil
guardar os valores com essa fórmula básica:</p>
<b>(n=0; n+1); VALOR = 2<sup>n</sup></b>
<p align='justify'>Isto é:</p><br>
2 elevado a 0 = 1;<br>
2 elevado a 1 = 2;<br>
2 elevador a 2 = 4;<br>
e assim por diante.
<p align='justify'>Já o tempo é justamente o tempo da nota. Não estão incluídas fusa e semifusa
 nessa tabela porque não usamos em nosso hinário.</p>

<p align='justify'>Outra coisa interessante é que o tempo médio dos hinos é 60BPM (Batidas Por Minuto).
 Hinos com ~120BPM indicam Minima como a nota de 1 tempo, enquanto hinos com ~60BPM indicam a Seminima.<p>

<p align='justify'>Lembre-se: Não é uma regra, mas esse andamento é considerado por causa do canto em
harmonia.</p>

<p align='justify'>No próximo item da Bag você encontrará uma calculadora de compasso, que auxiliará a
validar os exercícios de compasso. Mas use para validar, ou quando não tiver certeza, senão você não
aprenderá.</p>

<p align='justify'></p>

)";


    QScroller::grabGesture(ui->textBrowser_figuras->viewport(), QScroller::TouchGesture);
    ui->textBrowser_figuras->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->textBrowser_figuras->setStyleSheet(
        "QScrollBar:vertical{width:16px;margin:0px;}"
        "QScrollBar::handle:vertical{min-height:24px;border-radius:8px;background:#888;}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical{height:0;}"
        );
    ui->textBrowser_figuras->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->textBrowser_figuras->setReadOnly(true);
    ui->textBrowser_figuras->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->textBrowser_figuras->setHtml(html);

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
        lay2->addWidget(metro, 0, Qt::AlignVCenter);
    }


    //metro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    //ui->frameMetro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    ui->beatSlider->setDiscreteValues({2,3,4});   // já é o padrão
    ui->beatSlider->setValue(4);                  // começa em 4/4
    ui->beatSlider->setShowLabels(true);          // mostra “2  3  4” abaixo

    // tema opcional (coincide com seu app)
    ui->beatSlider->setColors(QColor("#2A2A2E"),  // trilho
                              QColor("#0B3D0B"),  // ativo
                              QColor("#B0B0B0"),  // ticks
                              QColor("#A8FF00"),  // handle
                              QColor("#E0E0E0")); // labels

    connect(ui->beatSlider, &BeatSlider::valueChanged, this, [this](int beats){
        metro->setBeatsPerMeasure(beats);
    });

    b_group = new QButtonGroup(this);

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


    ui->pushButton_stop->setText("Stop");
    ui->pushButton_stop->setIcon(QIcon(":/imgs/stop.png"));
    ui->pushButton_stop->setIconSize(QSize(16, 16));

    ui->pushButton_start->setText("Start");
    ui->pushButton_start->setIcon(QIcon(":/imgs/play.png"));
    ui->pushButton_start->setIconSize(QSize(16, 16));

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

    ui->pushButton_octave_down->setIcon(QIcon(":/imgs/arrowD.png"));
    ui->pushButton_octave_down->setIconSize(QSize(16, 16));
    ui->pushButton_octave_down->setProperty("moreOrLess",-1);
    connect(ui->pushButton_octave_down, &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveDown);

    ui->pushButton_octave_up->setIcon(QIcon(":/imgs/arrowU.png"));
    ui->pushButton_octave_up->setIconSize(QSize(16, 16));
    ui->pushButton_octave_up->setProperty("moreOrLess",1);
    connect(ui->pushButton_octave_up,   &QPushButton::clicked, this->toneGen, &ToneGenerator::octaveUp);

    ui->pushButton_previous->setIcon(QIcon(":/imgs/arrowL.png"));
    ui->pushButton_previous->setIconSize(QSize(16, 16));
    ui->pushButton_previous->setProperty("moreOrLess",-1);
    connect(ui->pushButton_previous, SIGNAL(clicked(bool)),this, SLOT(emitNote()));

    ui->pushButton_next->setIcon(QIcon(":/imgs/arrowR.png"));
    ui->pushButton_next->setIconSize(QSize(16, 16));
    ui->pushButton_next->setProperty("moreOrLess",1);
    connect(ui->pushButton_next, SIGNAL(clicked(bool)),this, SLOT(emitNote()));

    ui->pushButton_stop_2->setText("");
    ui->pushButton_stop_2->setIcon(QIcon(":/imgs/stop.png"));
    ui->pushButton_stop_2->setIconSize(QSize(16, 16));
    connect(ui->pushButton_stop_2, &QPushButton::clicked, toneGen, &ToneGenerator::stop);

    ui->pushButton_start_2->setText("");
    ui->pushButton_start_2->setIcon(QIcon(":/imgs/play.png"));
    ui->pushButton_start_2->setIconSize(QSize(16, 16));
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
    ui->stackWidget->setCurrentIndex(2);
    ui->toolBox->setCurrentIndex(PAGEINFO);
    ui->stackWidget->setCurrentIndex(1);
    ui->stackWidget->setCurrentIndex(0);

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

void MainWindow::setupStackNavigation()
{
    // ← botão voltar
    connect(ui->pushButton_stackBack, &QPushButton::clicked, this, [this]{
        auto *sw = ui->stackWidget;
        if (!sw || sw->count() == 0) return;
        int i = sw->currentIndex();
        // retrocede (com wrap para a última página)
        i = (i <= 0) ? (sw->count() - 1) : (i - 1);
        sw->setCurrentIndex(i);
    });

    // → botão avançar
    connect(ui->pushButton_stackNext, &QPushButton::clicked, this, [this]{
        auto *sw = ui->stackWidget;
        if (!sw || sw->count() == 0) return;
        int i = sw->currentIndex();
        // avança (com wrap para a primeira página)
        i = (i + 1) % sw->count();
        sw->setCurrentIndex(i);
    });

    // Sempre que trocar de página, atualiza o lineEdit
    connect(ui->stackWidget, &QStackedWidget::currentChanged,
            this, [this](int){ updateStackTitle(); });
}

QString MainWindow::currentPageTitle() const
{
    const auto *sw = ui->stackWidget;
    if (!sw) return {};

    QWidget *w = sw->currentWidget();
    if (!w) return {};

    // 1) tente uma propriedade "title" (útil para nomear no código/Designer)
    if (w->property("title").isValid())
        return w->property("title").toString();

    // 2) tente windowTitle (pode ser definido no Designer)
    if (!w->windowTitle().isEmpty())
        return w->windowTitle();

    // 3) fallback: objectName
    return w->objectName();
}

void MainWindow::updateStackTitle() const
{
    const QString t = const_cast<MainWindow*>(this)->currentPageTitle();
    // se quiser o lineEdit somente leitura:
    // ui->LineEdit_titles->setReadOnly(true);
    ui->lineEdit_titles->setText(t);
}


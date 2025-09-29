#include "StaffNoteWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QtMath>

// ======= util música =======
static inline int clampMidi(int m){ return qBound(0, m, 127); }

int StaffNoteWidget::midiFromNote(int noteIdx, int accidental, int octave)
{
    // C..B como 0..6
    static const int baseSemi[7] = {0,2,4,5,7,9,11}; // C D E F G A B
    noteIdx = qBound(0, noteIdx, 6);
    int semi = baseSemi[noteIdx] + qBound(-1, accidental, 1);
    int oct  = octave;
    if (semi >= 12) { semi -= 12; ++oct; }
    if (semi < 0)   { semi += 12; --oct; }
    return (oct + 1) * 12 + semi; // MIDI: C-1 = 0
}

double StaffNoteWidget::freqFromMidi(int midi)
{
    return 440.0 * std::pow(2.0, (clampMidi(midi)-69)/12.0);
}

int StaffNoteWidget::midiFromFreq(double hz)
{
    if (hz <= 0.0) return 69;
    double m = 69.0 + 12.0 * std::log2(hz/440.0);
    return int(std::lround(m));
}

int StaffNoteWidget::letterIndexForPcSharps(int pc)
{
    // pc: 0..11 => letra C..B (preferindo sustenidos)
    static const int map[12] = {
        0, // C
        0, // C# -> C
        1, // D
        1, // D# -> D
        2, // E
        3, // F
        3, // F# -> F
        4, // G
        4, // G# -> G
        5, // A
        5, // A# -> A
        6  // B
    };
    return map[qBound(0, pc, 11)];
}

int StaffNoteWidget::letterIndexForPcFlats(int pc)
{
    // preferindo bemóis
    static const int map[12] = {
        0, // C
        1, // Db -> D
        1, // D
        2, // Eb -> E
        2, // E
        3, // F
        4, // Gb -> G
        4, // G
        5, // Ab -> A
        5, // A
        6, // Bb -> B
        6  // B
    };
    return map[qBound(0, pc, 11)];
}

int StaffNoteWidget::letterIndexFromNameQChar(QChar c)
{
    switch (c.toUpper().unicode()){
    case 'C': return 0; case 'D': return 1; case 'E': return 2;
    case 'F': return 3; case 'G': return 4; case 'A': return 5;
    case 'B': return 6;
    }
    return 0;
}

int StaffNoteWidget::diatonicStepFrom(int letterIndex, int octave)
{
    // passo diatônico relativo a E4 (linha inferior = 0)
    // E=2, então: (letter-2) + 7*(oct-4)
    return (letterIndex - 2) + 7*(octave - 4);
}

QString StaffNoteWidget::nameFromMidiSharps(int midi)
{
    static const char* names[12] =
        {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int pc = (clampMidi(midi) % 12);
    int oct = clampMidi(midi)/12 - 1;
    return QString::fromLatin1(names[pc]) + QString::number(oct);
}

QString StaffNoteWidget::nameFromMidiFlats(int midi)
{
    static const char* names[12] =
        {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    int pc = (clampMidi(midi) % 12);
    int oct = clampMidi(midi)/12 - 1;
    return QString::fromLatin1(names[pc]) + QString::number(oct);
}

bool StaffNoteWidget::fontHasAccidentals(const QFont &f)
{
    QFontMetricsF fm(f);
    // tenta medir ♯ (U+266F) e ♭ (U+266D); se largura é ~0, provavelmente não há
    return fm.horizontalAdvance(QString::fromUtf8("♯")) > 0.1
           && fm.horizontalAdvance(QString::fromUtf8("♭")) > 0.1;
}

// ======= API =======
StaffNoteWidget::StaffNoteWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(240, 120);
}

void StaffNoteWidget::setMidi(int midi, AccPref pref)
{
    m_midi = clampMidi(midi);
    if (pref != AccPref::Auto) m_pref = pref;
    update();
}

void StaffNoteWidget::setFrequency(double hz, AccPref pref)
{
    setMidi(midiFromFreq(hz), pref);
}

void StaffNoteWidget::setNote(int noteIndex, int accidental, int octave, AccPref pref)
{
    setMidi(midiFromNote(noteIndex, accidental, octave), pref);
}

void StaffNoteWidget::setPreferAccidentals(AccPref pref)
{
    m_pref = pref;
    update();
}

void StaffNoteWidget::setShowLabel(bool on)
{
    m_showLabel = on;
    update();
}

void StaffNoteWidget::setColors(QColor bg, QColor staff, QColor note, QColor accent, QColor text)
{
    m_bg = bg; m_staff = staff; m_note = note; m_accent = accent; m_text = text;
    update();
}

// ======= layout & pintura =======
void StaffNoteWidget::computeLayout(QRectF &staffRect, qreal &lineGap, QRectF &leftGutter) const
{
    const qreal pad = qMin(width(), height()) * 0.08;
    // opção A (recomendada – mantém precisão em qreal)
    QRectF content = QRectF(rect()).marginsRemoved(QMarginsF(pad, pad, pad, pad));

    // reserva ~22% à esquerda para a clave
    leftGutter = QRectF(content.left(), content.top(), content.width()*0.22, content.height());
    staffRect  = QRectF(leftGutter.right() + pad*0.25, content.top(),
                       content.width() - leftGutter.width() - pad*0.25, content.height());

    // gap entre linhas (ajusta p/ caber 5 linhas confortáveis)
    lineGap = qBound(8.0, staffRect.height() / 6.0, 28.0);

    // verticalmente centraliza o pentagrama (4*gap de altura entre 1ª e 5ª linhas)
    const qreal staffHeight = 4*lineGap;
    const qreal y0 = staffRect.center().y() - staffHeight/2.0;
    staffRect.setTop(y0);
    staffRect.setBottom(y0 + staffHeight);
}

void StaffNoteWidget::setClefImage(const QPixmap& pm)
{
    m_clef = pm;
    update();
}

void StaffNoteWidget::setClefImageFile(const QString& path)
{
    QPixmap pm(path);
    if (!pm.isNull()) {
        m_clef = pm;
        update();
    }
}

void StaffNoteWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), m_bg);

    QRectF staffRect, left;
    qreal gap = 0;
    computeLayout(staffRect, gap, left);

    drawStaff(p, staffRect, gap);
    drawTrebleClef(p, QRectF(left.left(), staffRect.top()-gap*1.2, left.width(), staffRect.height()+gap*2.4), gap);
    drawNoteAndLedger(p, staffRect, gap);

    // rótulo da nota (canto superior direito)
    if (m_showLabel) {
        p.setPen(m_text);
        QFont f = font(); f.setBold(true);
        f.setPointSizeF(qMax(10.0, height()*0.12));
        p.setFont(f);
        QString label = (m_pref == AccPref::Flats) ? nameFromMidiFlats(m_midi) : nameFromMidiSharps(m_midi);
        p.drawText(rect().adjusted(0, 2, -8, 0), Qt::AlignTop | Qt::AlignRight, label);
    }
}

void StaffNoteWidget::drawStaff(QPainter &p, QRectF area, qreal lineGap)
{
    p.setPen(QPen(m_staff, 1.4, Qt::SolidLine, Qt::RoundCap));
    for (int i=0; i<5; ++i) {
        const qreal y = area.top() + i*lineGap;
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
}

void StaffNoteWidget::drawTrebleClef(QPainter &p, QRectF area, qreal lineGap)
{

    if (!m_clef.isNull()) {
        // alvo: ocupar vertical do staff com um pouco de folga
        QRectF target(area.left(), area.top(), area.width(), area.height());
        // preserva proporção
        const QSizeF imgSz = m_clef.size() / m_clef.devicePixelRatio();
        const qreal sx = target.width()  / imgSz.width();
        const qreal sy = target.height() / imgSz.height();
        const qreal s  = qMin(sx, sy);

        const QSizeF drawSz(imgSz.width()*s, imgSz.height()*s);
        const QPointF pos(target.left() + (target.width()  - drawSz.width())  / 2.0,
                          target.top()  + (target.height() - drawSz.height()) / 2.0);

        p.drawPixmap(QRectF(pos, drawSz), m_clef, QRectF(QPointF(0,0), imgSz));
        return; // já desenhou a clave via imagem
    }

    // Clave de Sol estilizada (path leve, sem fonte)
    // Foco: dar uma espiral que cruza a 2ª linha (G4)
    const qreal x = area.center().x();
    const qreal yMid = area.top() + 1*lineGap + lineGap; // linha 2 (G4)
    QPainterPath path;

    const qreal h = area.height();
    const qreal w = area.width()*0.45;
    const qreal r = lineGap*0.85;

    // haste
    path.moveTo(x + w*0.05, yMid - 2.5*lineGap);
    path.cubicTo(x + w*0.20, yMid - 3.2*lineGap,
                 x - w*0.35, yMid - 2.7*lineGap,
                 x - w*0.10, yMid - 1.6*lineGap);
    path.cubicTo(x + w*0.25, yMid - 0.6*lineGap,
                 x + w*0.15, yMid + 0.4*lineGap,
                 x - w*0.05, yMid + 0.9*lineGap);

    // espiral
    path.addEllipse(QPointF(x - r*0.1, yMid + 0.2*lineGap), r, r);

    // gancho inferior
    QPainterPath hook;
    hook.moveTo(x - r*0.9, yMid + 1.2*lineGap);
    hook.cubicTo(x - r*1.2, yMid + 2.0*lineGap,
                 x + r*0.8, yMid + 2.0*lineGap,
                 x + r*0.5, yMid + 0.9*lineGap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor("#C0C0C0"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.drawPath(hook);
}

void StaffNoteWidget::drawNoteAndLedger(QPainter &p, QRectF staffRect, qreal lineGap)
{
    // Mapeia MIDI -> letra & oitava conforme preferência
    const int midi = clampMidi(m_midi);
    const int pc   = midi % 12;
    const int oct  = midi/12 - 1;

    const int letter = (m_pref == AccPref::Flats)
                           ? letterIndexForPcFlats(pc)
                           : letterIndexForPcSharps(pc);

    const int step = diatonicStepFrom(letter, oct); // E4=0, F4=1, ..., F5=8

    // y da linha inferior (E4), e passo entre linhas/espaços
    const qreal yE4 = staffRect.bottom();
    const qreal stepGap = lineGap * 0.5; // cada passo diatônico é meia distância entre linhas

    // posição da nota (centro da cabeça)
    const qreal yNote = yE4 - step * stepGap;

    // cabeça da nota (oval levemente inclinada)
    const qreal d = lineGap * 1.2;
    QRectF headRect(staffRect.center().x() + staffRect.width()*0.18 - d/2.0, // um pouco à direita
                    yNote - d*0.40,
                    d*1.6,
                    d*1.05);

    p.save();
    p.translate(headRect.center());
    p.rotate(-18);
    QRectF r(-headRect.width()/2.0, -headRect.height()/2.0, headRect.width(), headRect.height());

    // preenchimento
    p.setPen(QPen(m_accent, 1.2));
    p.setBrush(m_note);
    p.drawEllipse(r);
    p.restore();

    // linhas suplementares (abaixo de E4 ou acima de F5)
    p.setPen(QPen(m_staff, 1.4, Qt::SolidLine, Qt::RoundCap));
    const qreal xL = headRect.left() - lineGap*0.4;
    const qreal xR = headRect.right() + lineGap*0.4;

    if (step < 0) {
        int sEven = (step % 2 == 0) ? step : step-1; // linha é passo par
        for (int s = -2; s >= sEven; s -= 2) {
            const qreal y = yE4 - s * stepGap;
            p.drawLine(QPointF(xL, y), QPointF(xR, y));
        }
    } else if (step > 8) {
        int sEven = (step % 2 == 0) ? step : step+1;
        for (int s = 10; s <= sEven; s += 2) {
            const qreal y = yE4 - s * stepGap;
            p.drawLine(QPointF(xL, y), QPointF(xR, y));
        }
    }

    // acidente (♯/♭) à esquerda da cabeça (se necessário)
    // decide se precisa ♯/♭ observando o nome textual pela preferência
    QString name = (m_pref == AccPref::Flats) ? nameFromMidiFlats(midi)
                                              : nameFromMidiSharps(midi);
    bool sharp = name.contains("♯") || name.contains('#');
    bool flat  = name.contains("♭") || name.contains('b');

    if (sharp || flat) {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(10.0, lineGap*1.2));
        p.setFont(f);
        p.setPen(m_text);

        const QString sym = sharp ? QString::fromUtf8("♯") : QString::fromUtf8("♭");
        QFontMetricsF fm(f);
        QString drawSym = sym;

        // fallback caso a fonte não tenha os símbolos
        if (!fontHasAccidentals(f)) {
            drawSym = sharp ? "#" : "b";
        }

        const qreal xAcc = headRect.left() - fm.horizontalAdvance(drawSym) - lineGap*0.4;
        p.drawText(QPointF(xAcc, yNote + fm.ascent()/2.8), drawSym);
    }
}

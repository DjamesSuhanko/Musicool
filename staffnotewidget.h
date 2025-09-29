#pragma once
#include <QWidget>
#include <QColor>
#include <QPixmap>

class StaffNoteWidget : public QWidget
{
    Q_OBJECT
public:
    enum class AccPref { Auto, Sharps, Flats };
    Q_ENUM(AccPref)

    explicit StaffNoteWidget(QWidget *parent = nullptr);
    ~StaffNoteWidget() override = default;

    // entrada por MIDI (A4=69)
    void setMidi(int midi, AccPref pref = AccPref::Auto);
    int  midi() const { return m_midi; }

    // entrada por frequência (Hz) — arredonda ao semitom mais próximo
    void setFrequency(double hz, AccPref pref = AccPref::Auto);

    // entrada por nota (0=C..6=B), acidente (-1=b,0=♮,1=#), oitava (C4=60 ⇒ octave=4)
    void setNote(int noteIndex, int accidental, int octave, AccPref pref = AccPref::Auto);

    // preferência de acidentes (se Auto, usa o que veio de setNote; senão força mapeamento)
    void setPreferAccidentals(AccPref pref);
    AccPref preferAccidentals() const { return m_pref; }

    // mostra "C#4" próximo da nota
    void setShowLabel(bool on);
    bool showLabel() const { return m_showLabel; }

    // paleta
    void setColors(QColor bg, QColor staff, QColor note, QColor accent, QColor text);

    void setClefImage(const QPixmap& pm);
    void setClefImageFile(const QString& path); // ":/sol.png"

protected:
    void paintEvent(QPaintEvent *e) override;
    QSize sizeHint() const override { return { 560, 200 }; }

private:
    // helpers musicais
    static int   midiFromNote(int noteIdx, int accidental, int octave);
    static double freqFromMidi(int midi);
    static int   midiFromFreq(double hz);
    static QString nameFromMidiSharps(int midi); // "C#4"
    static QString nameFromMidiFlats (int midi); // "Db4"

    // staff mapping
    static int   letterIndexForPcSharps(int pc); // C..B → 0..6 (preferindo sustenidos)
    static int   letterIndexForPcFlats (int pc); // idem, preferindo bemois
    static int   letterIndexFromNameQChar(QChar c); // 'C'..'B' → 0..6
    static int   diatonicStepFrom(int letterIndex, int octave);
    static bool  fontHasAccidentals(const QFont &f);

    // desenho
    void drawStaff(QPainter &p, QRectF area, qreal lineGap);
    void drawTrebleClef(QPainter &p, QRectF area, qreal lineGap);
    void drawNoteAndLedger(QPainter &p, QRectF staffRect, qreal lineGap);
    void computeLayout(QRectF &staffRect, qreal &lineGap, QRectF &leftGutter) const;

    QPixmap m_clef;  // se vazio, desenha a clave vetorial de fallback

public slots:
    void setFrequencyHz(double hz) { setFrequency(hz, AccPref::Sharps); }

private:
    int     m_midi = 69;               // A4 default
    AccPref m_pref = AccPref::Sharps;  // padrão “mais comum”
    bool    m_showLabel = true;

    // cores (dark)
    QColor  m_bg      = QColor("#121212");
    QColor  m_staff   = QColor("#3C3C40");
    QColor  m_note    = QColor("#EEEEEE");
    QColor  m_accent  = QColor("#4F8AFF");   // contorno/destaque
    QColor  m_text    = QColor("#E0E0E0");
};

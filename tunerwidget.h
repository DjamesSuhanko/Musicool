#pragma once
#include <QWidget>
#include <QColor>
class QPropertyAnimation;

class TunerWidget : public QWidget
{
    Q_OBJECT
    // valores
    Q_PROPERTY(double cents READ cents WRITE setCents NOTIFY centsChanged)
    Q_PROPERTY(double displayCents READ displayCents WRITE setDisplayCents NOTIFY displayCentsChanged)
    Q_PROPERTY(double minCents READ minCents WRITE setMinCents)
    Q_PROPERTY(double maxCents READ maxCents WRITE setMaxCents)
    Q_PROPERTY(double safeBandCents READ safeBandCents WRITE setSafeBandCents)
    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled)
    Q_PROPERTY(bool glowEnabled READ glowEnabled WRITE setGlowEnabled)

    // notas
    Q_PROPERTY(int baseMidi READ baseMidi WRITE setBaseMidi NOTIFY baseMidiChanged)       // nota alvo (ex.: A4=69)
    Q_PROPERTY(bool preferSharps READ preferSharps WRITE setPreferSharps)                 // C♯ em vez de D♭
    Q_PROPERTY(bool showOctave READ showOctave WRITE setShowOctave)
    Q_PROPERTY(bool showTopNote READ showTopNote WRITE setShowTopNote)
    Q_PROPERTY(bool showNumericTicks READ showNumericTicks WRITE setShowNumericTicks)
    Q_PROPERTY(bool showNoteMarkers READ showNoteMarkers WRITE setShowNoteMarkers)

    // paleta
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor trackBorderColor READ trackBorderColor WRITE setTrackBorderColor)
    Q_PROPERTY(QColor safeZoneColor READ safeZoneColor WRITE setSafeZoneColor)
    Q_PROPERTY(QColor tickColor READ tickColor WRITE setTickColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor)
    Q_PROPERTY(QColor glowColor READ glowColor WRITE setGlowColor)
    Q_PROPERTY(QColor noteMarkerColor READ noteMarkerColor WRITE setNoteMarkerColor)

public:
    explicit TunerWidget(QWidget* parent=nullptr);

    // valores
    double cents() const { return m_cents; }
    double displayCents() const { return m_displayCents; }
    double minCents() const { return m_minCents; }
    double maxCents() const { return m_maxCents; }
    double safeBandCents() const { return m_safeBand; }
    bool animationEnabled() const { return m_animEnabled; }
    bool glowEnabled() const { return m_glowEnabled; }

    // notas
    int  baseMidi() const { return m_baseMidi; }
    bool preferSharps() const { return m_preferSharps; }
    bool showOctave() const { return m_showOctave; }
    bool showTopNote() const { return m_showTopNote; }
    bool showNumericTicks() const { return m_showNumericTicks; }
    bool showNoteMarkers() const { return m_showNoteMarkers; }

    // paleta
    QColor backgroundColor() const { return m_bg; }
    QColor trackColor() const { return m_track; }
    QColor trackBorderColor() const { return m_trackBorder; }
    QColor safeZoneColor() const { return m_safe; }
    QColor tickColor() const { return m_tick; }
    QColor textColor() const { return m_text; }
    QColor indicatorColor() const { return m_indicator; }
    QColor glowColor() const { return m_glow; }
    QColor noteMarkerColor() const { return m_noteMarker; }

public slots:
    // valores
    void setCents(double c);
    void setDisplayCents(double c);
    void setMinCents(double v);
    void setMaxCents(double v);
    void setSafeBandCents(double v);
    void setAnimationEnabled(bool on);
    void setGlowEnabled(bool on);

    // notas
    void setBaseMidi(int midi);
    void setPreferSharps(bool v);
    void setShowOctave(bool v);
    void setShowTopNote(bool v);
    void setShowNumericTicks(bool v);
    void setShowNoteMarkers(bool v);

    // paleta
    void setBackgroundColor(const QColor& c);
    void setTrackColor(const QColor& c);
    void setTrackBorderColor(const QColor& c);
    void setSafeZoneColor(const QColor& c);
    void setTickColor(const QColor& c);
    void setTextColor(const QColor& c);
    void setIndicatorColor(const QColor& c);
    void setGlowColor(const QColor& c);
    void setNoteMarkerColor(const QColor& c);

signals:
    void centsChanged(double);
    void displayCentsChanged(double);
    void baseMidiChanged(int);

protected:
    QSize sizeHint() const override { return {560, 180}; }
    void paintEvent(QPaintEvent*) override;

private:
    QRectF trackRect() const;
    double valueToRatio(double c) const;   // 0..1
    static QString noteNameForMidi(int midi, bool preferSharps, bool withOctave);

    // estado
    double m_minCents = -50.0;
    double m_maxCents =  50.0;
    double m_safeBand =   5.0;
    double m_cents = 0.0;                  // alvo externo
    double m_displayCents = 0.0;           // renderizado (animado)

    // notas
    int  m_baseMidi = 69;                  // A4
    bool m_preferSharps = true;
    bool m_showOctave = true;
    bool m_showTopNote = true;
    bool m_showNumericTicks = true;
    bool m_showNoteMarkers = true;

    // animação
    bool m_animEnabled = true;
    QPropertyAnimation* m_anim = nullptr;

    // visual
    bool   m_glowEnabled = true;
    QColor m_bg{  0x12,0x12,0x12};
    QColor m_track{0x26,0x26,0x2a};
    QColor m_trackBorder{0x3c,0x3c,0x40};
    QColor m_safe{ 0x22,0x8b,0x22};
    QColor m_tick{ 0xaa,0xaa,0xaa};
    QColor m_text{ 0xee,0xee,0xee};
    QColor m_indicator{0x4f,0x8a,0xff};
    QColor m_glow{0x4f,0x8a,0xff};
    QColor m_noteMarker{0xff,0xff,0xff};
};

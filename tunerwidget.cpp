#include "tunerwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtMath>
#include <QPainterPath>           // <-- necessário
#include <QRadialGradient>        // <-- (usa QRadialGradient no desenho)

static double clampd(double v, double a, double b) {
    return v < a ? a : (v > b ? b : v);
}

TunerWidget::TunerWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    setMinimumSize(320, 110);

    m_anim = new QPropertyAnimation(this, "displayCents");
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    m_anim->setDuration(120);

    QPalette p = palette();
    p.setColor(QPalette::Window, m_bg);
    setPalette(p);
}

// ---------- setters (valores)
void TunerWidget::setCents(double c)
{
    c = clampd(c, m_minCents, m_maxCents);
    if (qFuzzyCompare(m_cents, c)) return;
    m_cents = c;
    emit centsChanged(c);

    if (m_animEnabled) {
        m_anim->stop();
        m_anim->setStartValue(m_displayCents);
        m_anim->setEndValue(c);
        int dur = int(clampd(std::abs(m_displayCents - c) * 2.0, 60.0, 160.0));
        m_anim->setDuration(dur);
        m_anim->start();
    } else {
        setDisplayCents(c);
    }
}
void TunerWidget::setDisplayCents(double c)
{
    c = clampd(c, m_minCents, m_maxCents);
    if (qFuzzyCompare(m_displayCents, c)) return;
    m_displayCents = c;
    emit displayCentsChanged(c);
    update();
}
void TunerWidget::setMinCents(double v){ m_minCents=v; update(); }
void TunerWidget::setMaxCents(double v){ m_maxCents=v; update(); }
void TunerWidget::setSafeBandCents(double v){ m_safeBand=std::abs(v); update(); }
void TunerWidget::setAnimationEnabled(bool on){ m_animEnabled=on; }
void TunerWidget::setGlowEnabled(bool on){ m_glowEnabled=on; update(); }

// ---------- setters (notas)
void TunerWidget::setBaseMidi(int midi){
    if (midi < 0) midi = 0;
    if (midi > 127) midi = 127;
    if (m_baseMidi == midi) return;
    m_baseMidi = midi;
    emit baseMidiChanged(m_baseMidi);
    update();
}
void TunerWidget::setPreferSharps(bool v){ m_preferSharps=v; update(); }
void TunerWidget::setShowOctave(bool v){ m_showOctave=v; update(); }
void TunerWidget::setShowTopNote(bool v){ m_showTopNote=v; update(); }
void TunerWidget::setShowNumericTicks(bool v){ m_showNumericTicks=v; update(); }
void TunerWidget::setShowNoteMarkers(bool v){ m_showNoteMarkers=v; update(); }

// ---------- setters (paleta)
void TunerWidget::setBackgroundColor(const QColor& c){ m_bg=c; update(); }
void TunerWidget::setTrackColor(const QColor& c){ m_track=c; update(); }
void TunerWidget::setTrackBorderColor(const QColor& c){ m_trackBorder=c; update(); }
void TunerWidget::setSafeZoneColor(const QColor& c){ m_safe=c; update(); }
void TunerWidget::setTickColor(const QColor& c){ m_tick=c; update(); }
void TunerWidget::setTextColor(const QColor& c){ m_text=c; update(); }
void TunerWidget::setIndicatorColor(const QColor& c){ m_indicator=c; update(); }
void TunerWidget::setGlowColor(const QColor& c){ m_glow=c; update(); }
void TunerWidget::setNoteMarkerColor(const QColor& c){ m_noteMarker=c; update(); }

// ---------- helpers
QRectF TunerWidget::trackRect() const
{
    const int m = qRound(qMin(width(), height()) * 0.10);
    QRectF r = rect().marginsRemoved(QMargins(m, m, m, m));
    const qreal h = qMin<qreal>(r.height()*0.38, 50.0);
    const qreal y = r.center().y() - h/2.0;
    return QRectF(r.left(), y, r.width(), h);
}
double TunerWidget::valueToRatio(double c) const
{
    const double range = (m_maxCents - m_minCents);
    if (range <= 0.0) return 0.5;
    return (c - m_minCents) / range;
}
QString TunerWidget::noteNameForMidi(int midi, bool preferSharps, bool withOctave)
{
    static const char* sharpNames[12] = {"C","C♯","D","D♯","E","F",
                                         "F♯","G","G♯","A","A♯","B"};
    static const char* flatNames [12] = {"C","D♭","D","E♭","E","F",
                                        "G♭","G","A♭","A","B♭","B"};
    if (midi < 0) midi = 0; if (midi > 127) midi = 127;
    int pc = midi % 12;
    int oct = midi / 12 - 1;
    const char* base = preferSharps ? sharpNames[pc] : flatNames[pc];
    QString s = QString::fromUtf8(base);
    if (withOctave) s += QString::number(oct);
    return s;
}

// ---------- paint
void TunerWidget::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);

    // fundo
    g.fillRect(rect(), m_bg);

    const QRectF tr = trackRect();
    const qreal radius = tr.height()*0.5;

    // trilho
    {
        QPainterPath path;
        path.addRoundedRect(tr, radius, radius);
        g.fillPath(path, m_track);
        QPen pen(m_trackBorder, 1.2);
        pen.setCosmetic(true);
        g.setPen(pen);
        g.drawPath(path);
    }

    // zona segura ±safeBand
    {
        const double safeWidthRatio = (m_safeBand * 2.0) / (m_maxCents - m_minCents);
        const qreal w = tr.width() * safeWidthRatio;
        const QRectF safeRect(tr.center().x() - w/2.0, tr.top(), w, tr.height());
        QPainterPath p; p.addRoundedRect(safeRect, radius, radius);
        QColor c = m_safe; c.setAlpha(85);
        g.fillPath(p, c);
    }

    // ticks numéricos
    if (m_showNumericTicks) {
        auto drawTick = [&](double val, int len, int thick, const QString& label){
            const double r = valueToRatio(val);
            const qreal x = tr.left() + r * tr.width();
            QPen pen(m_tick, thick);
            pen.setCosmetic(true);
            g.setPen(pen);
            g.drawLine(QPointF(x, tr.bottom()+6), QPointF(x, tr.bottom()+6+len));
            if (!label.isEmpty()) {
                QFont f = font(); f.setPointSizeF(qMax(9.0, height()*0.07));
                g.setFont(f);
                g.setPen(m_text);
                QRectF tb(x-40, tr.bottom()+8+len, 80, height()*0.18);
                g.drawText(tb, Qt::AlignHCenter|Qt::AlignTop, label);
            }
        };
        drawTick(-50, 10, 2, "-50");
        drawTick(-25, 7,  1, "-25");
        drawTick(  0, 14, 3,  "0");
        drawTick( 25, 7,  1, "25");
        drawTick( 50, 10, 2, "50");
    }

    // marcadores de NOTA (−50 / 0 / +50)
    if (m_showNoteMarkers) {
        const QString leftNote  = noteNameForMidi(m_baseMidi - 1, m_preferSharps, m_showOctave);
        const QString midNote   = noteNameForMidi(m_baseMidi,     m_preferSharps, m_showOctave);
        const QString rightNote = noteNameForMidi(m_baseMidi + 1, m_preferSharps, m_showOctave);

        auto drawLabel = [&](double val, const QString& text, bool emphasize){
            const double r = valueToRatio(val);
            const qreal x = tr.left() + r * tr.width();
            QFont f = font();
            f.setBold(emphasize);
            f.setPointSizeF(qMax(10.0, height() * (emphasize ? 0.12 : 0.095)));
            g.setFont(f);
            g.setPen(emphasize ? m_noteMarker : m_noteMarker.darker(135));
            QRectF box(x-60, tr.top()-height()*0.22, 120, height()*0.18);
            g.drawText(box, Qt::AlignHCenter|Qt::AlignBottom, text);
        };

        drawLabel(-50, leftNote,  false);
        if (!m_showTopNote)       // << evita sobrepor com o título grande
            drawLabel(  0, midNote,   true);
        drawLabel( 50, rightNote, false);
    }

    // indicador (bolinha)
    {
        const double r = valueToRatio(m_displayCents);
        qreal x = tr.left() + r * tr.width();
        qreal y = tr.center().y();
        qreal R = tr.height()*0.48;

        if (m_glowEnabled) {
            for (int i=4; i>=1; --i) {
                qreal alpha = 50.0 * (i/4.0);
                QColor gc = m_glow; gc.setAlpha(int(alpha));
                g.setBrush(Qt::NoBrush);
                QPen gp(gc, R*0.35*(i/4.0)); gp.setCapStyle(Qt::RoundCap);
                g.setPen(gp);
                g.drawPoint(QPointF(x,y));
            }
        }

        QRadialGradient grad(QPointF(x,y), R);
        QColor c1 = m_indicator, c2 = m_indicator;
        c1.setAlpha(230); c2.setAlpha(180);
        grad.setColorAt(0.0, Qt::white);
        grad.setColorAt(0.15, c1.lighter(120));
        grad.setColorAt(1.0,  c2.darker(110));
        g.setPen(Qt::NoPen);
        g.setBrush(grad);
        g.drawEllipse(QPointF(x,y), R, R);

        g.setPen(QPen(m_trackBorder, 1.0));
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(QPointF(x,y), R, R);
    }

    // topo: nota grande + cents (opcional)
    if (m_showTopNote) {
        const QString midNote = noteNameForMidi(m_baseMidi, m_preferSharps, m_showOctave);
        QFont f = font(); f.setBold(true);
        f.setPointSizeF(qMax(12.0, height()*0.16));
        g.setFont(f);
        g.setPen(m_text);
        QRectF nbox(0, rect().top()+4, rect().width(), trackRect().top()-rect().top()-6);
        g.drawText(nbox, Qt::AlignHCenter|Qt::AlignVCenter, midNote);

        // cents pequeno ao lado (ou abaixo)
        QFont f2 = font(); f2.setPointSizeF(qMax(9.0, height()*0.09));
        g.setFont(f2);
        const QString centsStr = QString::number(m_displayCents, 'f', 1) + " ¢";
        QRectF cbox(0, nbox.bottom()-height()*0.05, rect().width(), height()*0.12);
        g.drawText(cbox, Qt::AlignHCenter|Qt::AlignTop, centsStr);
    }
}

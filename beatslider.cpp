#include "BeatSlider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

static inline int clampInt(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }

BeatSlider::BeatSlider(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TranslucentBackground, true); // sem fundo
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
}

void BeatSlider::setDiscreteValues(const QVector<int>& vals)
{
    if (vals.isEmpty()) return;
    QVector<int> sorted = vals;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    m_vals = sorted;
    if (!m_vals.contains(m_value)) m_value = m_vals.first();
    update();
}

void BeatSlider::setValue(int v)
{
    if (!m_vals.contains(v)) return;
    if (m_value == v) return;
    m_value = v;
    update();
    emit valueChanged(m_value);
}

void BeatSlider::setValueSilently(int v)
{
    if (!m_vals.contains(v)) return;
    m_value = v;
    update();
}

void BeatSlider::setColors(QColor track, QColor active, QColor tick, QColor handle, QColor text)
{
    m_track  = track;
    m_active = active;
    m_tick   = tick;
    m_handle = handle;
    m_text   = text;
    update();
}

QRectF BeatSlider::trackRect() const
{
    const qreal padX = qMax<qreal>(m_padX, m_handleRadius + 6.0); // knob não “vaza”
    const qreal padY = m_padY;

    QRectF r = rect();
    r.adjust(padX, padY, -padX, -padY);

    const qreal h = qreal(m_trackThickness);
    const qreal y = rect().center().y() - h/2.0;
    r.setTop(y);
    r.setBottom(y + h);
    return r;
}


QVector<QPointF> BeatSlider::tickPoints() const
{
    if (!m_tickPts.isEmpty() && m_tickPts.size() == m_vals.size()) return m_tickPts;

    const QRectF tr = trackRect();
    QVector<QPointF> pts; pts.reserve(m_vals.size());

    if (m_vals.size() == 1) {
        pts.push_back(QPointF(tr.center().x(), tr.center().y()));
    } else {
        const qreal step = tr.width() / qreal(m_vals.size() - 1);
        for (int i=0;i<m_vals.size();++i)
            pts.push_back(QPointF(tr.left() + i*step, tr.center().y()));
    }
    // guarda (mutable) — recalculamos tb em resizeEvent
    const_cast<BeatSlider*>(this)->m_tickPts = pts;
    return pts;
}

int BeatSlider::nearestIndexAtX(qreal x) const
{
    const auto pts = tickPoints();
    if (pts.isEmpty()) return -1;
    int best = 0;
    qreal bd = 1e18;
    for (int i=0;i<pts.size();++i){
        qreal d = std::abs(pts[i].x() - x);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void BeatSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // Nada de fillRect: fundo transparente

    const QRectF tr = trackRect();
    const auto pts  = tickPoints();

    // Trilho (verde escuro)
    p.setPen(Qt::NoPen);
    p.setBrush(m_track);
    const qreal radius = tr.height() * 0.45;
    p.drawRoundedRect(tr, radius, radius);

    // Parte ativa: mesma cor do trilho (evita qualquer azul)
    int idx = m_vals.indexOf(m_value);
    idx = clampInt(idx, 0, m_vals.size()-1);
    const qreal x0 = pts.first().x();
    const qreal x1 = pts[idx].x();
    QRectF activeRect(QPointF(qMin(x0,x1), tr.top()),
                      QPointF(qMax(x0,x1), tr.bottom()));
    p.setBrush(m_active);
    p.drawRoundedRect(activeRect, radius, radius);

    // Ticks
    p.setPen(QPen(m_tick, 2.0, Qt::SolidLine, Qt::RoundCap));
    for (const auto& pt : pts)
        p.drawLine(QPointF(pt.x(), tr.top()-6), QPointF(pt.x(), tr.bottom()+6));

    // Labels (opcional)
    if (m_showLabels) {
        QFont f = font(); f.setBold(true);
        f.setPointSizeF(qMax(10.0, height()*0.22));
        p.setFont(f);
        p.setPen(m_text);
        for (int i=0;i<m_vals.size();++i) {
            const QRectF bb(pts[i].x()-40, tr.bottom()+4, 80, height()*0.35);
            p.drawText(bb, Qt::AlignHCenter|Qt::AlignTop, QString::number(m_vals[i]));
        }
    }

    // Knob (respeita handleRadius)
    const QPointF hc(pts[idx].x(), tr.center().y());
    const qreal   rHandle = qreal(m_handleRadius);
    p.setBrush(QColor(0,0,0,60)); p.setPen(Qt::NoPen);
    p.drawEllipse(hc + QPointF(0,2), rHandle*0.9, rHandle*0.9);
    p.setBrush(m_handle);
    p.setPen(QPen(QColor(0,0,0,90), 1.2));
    p.drawEllipse(hc, rHandle, rHandle);
}


void BeatSlider::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    m_dragging = true;
    m_dragIndex = nearestIndexAtX(e->position().x());
    if (m_dragIndex >= 0 && m_dragIndex < m_vals.size()) {
        setValue(m_vals[m_dragIndex]);
    }
    e->accept();
}

void BeatSlider::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) return;
    int idx = nearestIndexAtX(e->position().x());
    if (idx != m_dragIndex && idx >= 0 && idx < m_vals.size()) {
        m_dragIndex = idx;
        setValue(m_vals[idx]);
    }
}

void BeatSlider::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_dragging) {
        m_dragging = false;
        int idx = nearestIndexAtX(e->position().x());
        idx = clampInt(idx, 0, m_vals.size()-1);
        setValue(m_vals[idx]); // snap final
        e->accept();
    }
}

void BeatSlider::resizeEvent(QResizeEvent*)
{
    m_tickPts.clear(); // força recálculo em tickPoints()
}

void BeatSlider::setTrackThickness(int px){ m_trackThickness = qMax(4, px); update(); }
void BeatSlider::setHandleRadius(int px)  { m_handleRadius   = qMax(10, px); update(); }
void BeatSlider::setHorizontalPadding(int px){ m_padX = qMax(0, px); update(); }
void BeatSlider::setVerticalPadding(int px){   m_padY = qMax(0, px); update(); }

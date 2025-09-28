#include "MetronomeWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QStyleOption>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioSink>
#include <QtMath>

// ---------------- ctor/dtor ----------------
MetronomeWidget::MetronomeWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumHeight(120);

    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MetronomeWidget::onBeat);
}

MetronomeWidget::~MetronomeWidget()
{
    stop();
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
}

// ---------------- parâmetros públicos ----------------
void MetronomeWidget::setBeatsPerMeasure(int beats)
{
    if (beats < 2) beats = 2;
    if (beats > 4) beats = 4;
    if (m_beats == beats) return;
    m_beats = beats;
    m_currentBeat = 0;
    update();
}

void MetronomeWidget::setBpm(int bpm)
{
    bpm = qBound(30, bpm, 300);
    if (m_bpm == bpm) return;
    m_bpm = bpm;
    if (m_running) {
        const int intervalMs = int(60000.0 / double(m_bpm));
        m_timer.start(intervalMs);
    }
    update();
}

void MetronomeWidget::setRunning(bool on)
{
    if (on) start(); else stop();
}

void MetronomeWidget::setAudioEnabled(bool on)
{
    m_audioOn = on;
}

void MetronomeWidget::setAccentEnabled(bool on)
{
    m_accentOn = on;
}

void MetronomeWidget::setVolume(float vol01)
{
    m_volume = qBound(0.0f, vol01, 1.0f);
    if (m_sink) m_sink->setVolume(m_volume);
}

void MetronomeWidget::setDownbeatHz(double hz)
{
    m_fDownbeat = qBound(100.0, hz, 2000.0);
    prepareClicks();
}

void MetronomeWidget::setUpbeatHz(double hz)
{
    m_fUpbeat = qBound(100.0, hz, 4000.0);
    prepareClicks();
}

// ---------------- controle ----------------
void MetronomeWidget::start()
{
    if (m_running) return;

    ensureAudio();
    m_currentBeat = m_beats - 1;                 // << prepara para virar para 0 no primeiro tick

    const int intervalMs = int(60000.0 / double(m_bpm));
    m_timer.start(intervalMs);
    m_running = true;

    onBeat();                                    // toca imediatamente o 1 (downbeat)
}

void MetronomeWidget::stop()
{
    if (!m_running) return;
    m_timer.stop();
    m_running = false;
    update();
}

// ---------------- áudio helpers ----------------
void MetronomeWidget::ensureAudio()
{
    if (!m_audioOn) return;

    // cria/recria a saída se necessário
    if (!m_sink) {
        QAudioDevice dev = QMediaDevices::defaultAudioOutput();
        QAudioFormat fmt;
        fmt.setSampleRate(m_sampleRate);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);

        if (!dev.isFormatSupported(fmt)) {
            fmt = dev.preferredFormat();
            m_sampleRate = fmt.sampleRate();
        }

        m_sink = new QAudioSink(dev, fmt, this);
        m_sink->setVolume(m_volume);
        m_out = m_sink->start();

        prepareClicks();
    } else if (!m_out) {
        m_out = m_sink->start();
    }
}

static QVector<qint16> genClick(int sr, double hz, int ms, float amp = 0.9f)
{
    const int N = qMax(1, (sr * ms) / 1000);
    QVector<qint16> out;
    out.resize(N);
    const double w = 2.0 * M_PI * hz / double(sr);

    // janela curta (fade in/out) para evitar estalos
    const int fade = qMax(1, sr / 1000 * 2); // ~2 ms
    for (int i=0; i<N; ++i) {
        double env = 1.0;
        if (i < fade) env = double(i) / fade;
        else if (i > N - 1 - fade) env = double(N - 1 - i) / fade;

        double s = amp * env * std::sin(w * i);
        s = qBound(-1.0, s, 1.0);
        out[i] = qint16(s * 32767.0);
    }
    return out;
}

void MetronomeWidget::prepareClicks()
{
    if (!m_sink) return; // será chamado no ensureAudio novamente
    const int sr = m_sink->format().sampleRate();
    // duração curta ~35 ms (não atrapalha BPM alto)
    m_clickDown = genClick(sr, m_fDownbeat, 40, 0.95f);
    m_clickUp   = genClick(sr, m_fUpbeat,   32, 0.85f);
}

void MetronomeWidget::playClick(bool downbeat)
{
    if (!m_audioOn || !m_sink || !m_out) return;

    const QVector<qint16>& v = downbeat ? m_clickDown : m_clickUp;
    if (v.isEmpty()) return;

    const char* ptr = reinterpret_cast<const char*>(v.constData());
    const int   len = v.size() * int(sizeof(qint16));
    m_out->write(ptr, len);
}

// ---------------- tick ----------------
void MetronomeWidget::onBeat()
{
    // 0,1,2,3...
    m_currentBeat = (m_currentBeat + 1) % m_beats;

    const bool isDown = (m_currentBeat == 0);
    if (m_audioOn) playClick(isDown && m_accentOn);

    emit tick(m_currentBeat, isDown);

    update();     // o repaint agora reflete exatamente a batida tocada
}


// ---------------- pintura ----------------
void MetronomeWidget::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);

    drawBackground(g);
    drawBeatSquares(g);

    // BPM no canto
    g.setPen(m_text);
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(qMax(9.0, height() * 0.13));
    g.setFont(f);
    const QString bpmTxt = QString::number(m_bpm) + " BPM";
    g.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, bpmTxt);
}

void MetronomeWidget::drawBackground(QPainter &g)
{
    g.fillRect(rect(), m_bg);
}

void MetronomeWidget::drawBeatSquares(QPainter &g)
{
    const int n = m_beats;

    // área útil
    const qreal m = qMin(width(), height()) * 0.10;
    QRectF area = QRectF(rect()).marginsRemoved(QMarginsF(m, m*1.3, m, m*0.8));



    // dimensões dos quadrados
    const qreal gap = qMax<qreal>(6.0, area.width() * 0.02);
    const qreal wTot = area.width() - gap * (n - 1);
    const qreal boxW = qMax<qreal>(40.0, wTot / n);
    const qreal boxH = qMin<qreal>(qMax<qreal>(50.0, area.height()), boxW * 0.9);
    const qreal y = area.center().y() - boxH / 2.0;
    const qreal radius = qMin(boxW, boxH) * 0.18;

    QPen pen(m_boxBorder, 1.2);
    for (int i = 0; i < n; ++i) {
        const qreal x = area.left() + i * (boxW + gap);

        // torna o “box” um círculo: usa o menor dos dois lados como diâmetro
        const qreal d  = qMin(boxW, boxH);
        const qreal yC = y + (boxH - d) / 2.0;
        QRectF r(x, yC, d, d);

        const bool active = (i == m_currentBeat);
        const bool down   = (i == 0);

        QColor fill = m_box;
        if (active) fill = down ? m_highlightDn : m_highlightUp;

        // fundo do círculo
        g.setPen(Qt::NoPen);
        g.setBrush(fill);
        g.drawEllipse(r);

        // borda
        g.setPen(pen);
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(r);

        // número do tempo (centralizado no círculo)
        QFont f = font();
        f.setBold(active);
        f.setPointSizeF(qMax(10.0, d * 0.40));
        g.setFont(f);
        g.setPen(QColor("#EEEEEE"));
        g.drawText(r, Qt::AlignCenter, QString::number(i + 1));
    }
}

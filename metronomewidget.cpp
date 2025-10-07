#include "MetronomeWidget.h"

#include <QPainter>
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QtMath>
#include <QIODevice>

// ------------------------------------
// Utilitários pequenos
static inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
// ------------------------------------

MetronomeWidget::MetronomeWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    // Ajusta paleta para as barras:
    // - m_box        -> verde bem escuro (barras ativas “em espera”)
    // - m_highlightDn-> verde limão (barra ativa no tick)
    // - m_boxBorder  -> não usamos borda nas barras, mas mantemos um cinza escuro
    m_bg        = QColor("#121212");
    m_box       = QColor("#0B3D0B");  // verde escuro
    m_highlightDn = QColor("#A8FF00"); // verde limão (ativa)
    m_boxBorder = QColor("#3A3A3A");  // cinza (usado para barras “fora do compasso”)

    // Timer
    connect(&m_timer, &QTimer::timeout, this, &MetronomeWidget::onBeat);
    m_timer.setTimerType(Qt::PreciseTimer);

    // Áudio pré-config
    ensureAudio();
    prepareClicks();
}

MetronomeWidget::~MetronomeWidget()
{
    stop();
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
}

void MetronomeWidget::setBeatsPerMeasure(int beats)
{
    beats = clampInt(beats, 2, 4);
    if (m_beats == beats) return;
    m_beats = beats;
    m_currentBeat = 0;
    update();
}

void MetronomeWidget::setBpm(int bpm)
{
    bpm = clampInt(bpm, 30, 300);
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
}

void MetronomeWidget::setDownbeatHz(double hz)
{
    m_fDownbeat = qMax(20.0, hz);
    prepareClicks();
}

void MetronomeWidget::setUpbeatHz(double hz)
{
    m_fUpbeat = qMax(20.0, hz);
    prepareClicks();
}

void MetronomeWidget::start()
{
    if (m_running) return;

    ensureAudio();
    prepareClicks();

    // Preparar para que a PRIMEIRA batida seja o tempo 1 (índice 0)
    m_currentBeat = m_beats - 1;

    const int intervalMs = int(60000.0 / double(m_bpm));
    m_timer.start(intervalMs);
    m_running = true;

    onBeat(); // toca e destaca imediatamente o tempo 1
}

void MetronomeWidget::stop()
{
    if (!m_running) return;
    m_timer.stop();
    m_running = false;
    update();
}

void MetronomeWidget::onBeat()
{
    // 0..beats-1 (incrementa ANTES e pinta/soa DEPOIS)
    m_currentBeat = (m_currentBeat + 1) % m_beats;

    const bool isDown = (m_currentBeat == 0);
    if (m_audioOn) playClick(isDown && m_accentOn);

    emit tick(m_currentBeat, isDown);
    update();
}

// ----------------------- DESENHO -----------------------

void MetronomeWidget::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);

    drawBackground(g);
    drawBeatSquares(g); // aqui implementamos as BARRAS (mantemos o nome da função)
}

void MetronomeWidget::drawBackground(QPainter &g)
{
    g.fillRect(rect(), m_bg);
}

void MetronomeWidget::drawBeatSquares(QPainter &g)
{
    const QRectF full = QRectF(rect()).marginsRemoved(QMarginsF(12, 12, 12, 12));

    // Agora é uma ÚNICA fileira horizontal de barras
    const int totalBars  = 4;                      // sempre desenhamos 4
    const int beatsInUse = qBound(1, m_beats, 4);  // quantas estão ativas no compasso

    // Geometria das barras: finas na vertical, lado a lado na horizontal
    const qreal gapX      = qMax<qreal>(8.0, full.width() * 0.03);
    const qreal barHeight = qMax<qreal>(10.0, full.height() * 0.18); // “estreita” verticalmente
    const qreal totalW    = full.width() - (totalBars - 1) * gapX;
    const qreal barWidth  = qMax<qreal>(12.0, totalW / totalBars);

    const qreal y = full.center().y() - barHeight / 2.0;
    const qreal r = qMin<qreal>(8.0, barHeight * 0.35); // canto arredondado leve

    g.setPen(Qt::NoPen);

    for (int i = 0; i < totalBars; ++i) {
        const qreal x = full.left() + i * (barWidth + gapX);
        const QRectF bar(x, y, barWidth, barHeight);

        QColor fill;
        if (i >= beatsInUse) {
            fill = m_boxBorder;               // barras “fora” quando compasso < 4 → cinza
        } else if (i == m_currentBeat) {
            fill = m_highlightDn;             // barra do tempo atual → verde limão
        } else {
            fill = m_box;                     // barras ativas em espera → verde escuro
        }

        g.setBrush(fill);
        g.drawRoundedRect(bar, r, r);
    }
}


// ----------------------- ÁUDIO -----------------------

void MetronomeWidget::ensureAudio()
{
    if (m_sink) return;

    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    QAudioFormat fmt;
    fmt.setSampleRate(m_sampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    if (!dev.isFormatSupported(fmt)) {
        fmt = dev.preferredFormat();
    }
    m_sampleRate = fmt.sampleRate();

    m_sink = new QAudioSink(dev, fmt, this);
    m_sink->setVolume(1.0f); // volume do dispositivo (0..1)

    // usamos modo push: gravamos os samples no QIODevice retornado
    m_out = m_sink->start();
}

void MetronomeWidget::prepareClicks()
{
    // Gera dois “clicks” curtos com envelope, para escrever em m_out no onBeat()
    // Duração ~60 ms; ataque curto, decaimento exponencial
    if (!m_sink) return;

    auto makeClick = [&](double freqHz)->QVector<qint16> {
        const int durMs   = 60;
        const int N       = int((m_sampleRate * durMs) / 1000.0);
        QVector<qint16> v; v.resize(qMax(1, N));

        const double twoPi = 2.0 * M_PI;
        const double w = twoPi * freqHz / double(m_sampleRate);

        // envelope: ataque 2 ms, sustain curto, release exponencial
        const int attack  = qMax(1, int(m_sampleRate * 0.002));
        const int release = qMax(1, N - attack);

        double amp = m_volume; // 0..1
        for (int n = 0; n < N; ++n) {
            float env = 1.0f;
            if (n < attack) {
                env = float(n) / float(attack); // ataque linear
            } else {
                const float t = float(n - attack) / float(qMax(1, release));
                // decaimento suave (exponencial aproximado)
                env = std::exp(-4.0f * t);
            }

            const double s = std::sin(w * n) * (amp * env);
            const int smp = int(qBound(-1.0, s, 1.0) * 32767.0);
            v[n] = qint16(smp);
        }
        return v;
    };

    m_clickDown = makeClick(m_fDownbeat);
    m_clickUp   = makeClick(m_fUpbeat);
}

void MetronomeWidget::playClick(bool downbeat)
{
    if (!m_sink || !m_out) return;

    const QVector<qint16>& src = downbeat ? m_clickDown : m_clickUp;
    if (src.isEmpty()) return;

    // escreve em modo push (bloqueio mínimo)
    const char* data = reinterpret_cast<const char*>(src.constData());
    const qint64 bytes = qint64(src.size() * int(sizeof(qint16)));
    qint64 written = 0;
    while (written < bytes) {
        const qint64 w = m_out->write(data + written, bytes - written);
        if (w <= 0) break; // evita loop infinito se algo der errado
        written += w;
    }
}

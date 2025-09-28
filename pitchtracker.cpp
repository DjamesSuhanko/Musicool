#include "PitchTracker.h"

#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioSource>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <QDebug>

PitchTracker::PitchTracker(QObject* parent)
    : QObject(parent)
{
    // Não definimos device/format aqui para não depender de permissão;
    // fazemos isso em start().
    m_fifo.reserve(m_sampleRate); // ~1s
}

PitchTracker::~PitchTracker()
{
    stop();
    if (m_source) {
        m_source->deleteLater();
        m_source = nullptr;
    }
}

// ----------------- Config -----------------
void PitchTracker::setMinFrequency(double hz)   { m_minF = std::max(10.0, hz); }
void PitchTracker::setMaxFrequency(double hz)   { m_maxF = std::max(20.0, hz); }
void PitchTracker::setAnalysisSize(int samples) { m_analysisSize = std::max(1024, samples); }
void PitchTracker::setProcessIntervalMs(int ms) { m_processInterval = std::max(10, ms); }
void PitchTracker::setSilenceRmsThreshold(double t) {
    m_silenceThresh = std::max(0.0, std::min(0.1, t));
}

// ----------------- Start/Stop -----------------
bool PitchTracker::start()
{
    if (m_running) return true;

    // (Re)descobre o dispositivo e o formato AGORA (perm já concedida)
    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!dev.isNull()) {
        QAudioFormat want;
        want.setSampleRate(m_sampleRate);
        want.setChannelCount(1);
        want.setSampleFormat(QAudioFormat::Int16);

        if (dev.isFormatSupported(want)) {
            m_fmt = want;
        } else {
            m_fmt = dev.preferredFormat();
            m_sampleRate = m_fmt.sampleRate();
            m_channels   = m_fmt.channelCount();
        }
    } else {
        // fallback sensato
        m_fmt.setSampleRate(m_sampleRate);
        m_fmt.setChannelCount(1);
        m_fmt.setSampleFormat(QAudioFormat::Int16);
    }

    // Sempre recrie a fonte para garantir estado limpo
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_source = new QAudioSource(dev, m_fmt, this);

    // Latência moderada (resposta estável p/ afinador)
    m_source->setBufferSize(std::max(4096, m_sampleRate / 10)); // ~100 ms

    m_io = m_source->start();
    if (!m_io) {
        qWarning() << "[PitchTracker] start() failed: QAudioSource::start() returned nullptr";
        return false;
    }

    connect(m_io, &QIODevice::readyRead, this, &PitchTracker::onReadyRead, Qt::UniqueConnection);

    m_timer.restart();
    m_running = true;
    emit started();

    qInfo() << "[PitchTracker] started at" << m_fmt.sampleRate() << "Hz,"
            << m_fmt.channelCount() << "ch, sf=" << int(m_fmt.sampleFormat());
    return true;
}

void PitchTracker::stop()
{
    if (!m_running) return;

    if (m_io) {
        disconnect(m_io, nullptr, this, nullptr);
        m_io = nullptr;
    }
    if (m_source) {
        m_source->stop();
        // não deletamos já — deixamos para o próximo start recriar limpo
        // (mas garantimos que o ponteiro seja destruído no dtor)
    }
    m_running = false;
    emit stopped();
    qInfo() << "[PitchTracker] stopped";
}

// ----------------- Audio fluxo -----------------
void PitchTracker::onReadyRead()
{
    if (!m_io) return;
    const qint64 avail = m_io->bytesAvailable();
    if (avail <= 0) return;

    QByteArray chunk;
    chunk.resize(int(avail));
    const qint64 read = m_io->read(chunk.data(), chunk.size());
    if (read <= 0) return;

    pushSamplesFromBytes(chunk.constData(), int(read));

    // Processa no máximo a cada m_processInterval ms
    if (m_timer.elapsed() >= m_processInterval) {
        processAnalysis();
        m_timer.restart();
    }
}

void PitchTracker::pushSamplesFromBytes(const char* data, int bytes)
{
    const int ch = m_fmt.channelCount();

    switch (m_fmt.sampleFormat()) {
    case QAudioFormat::Int16: {
        const qint16* p = reinterpret_cast<const qint16*>(data);
        const int frames = bytes / (int(sizeof(qint16)) * ch);
        for (int i=0; i<frames; ++i) {
            float s = 0.f;
            for (int c=0; c<ch; ++c)
                s += p[i*ch + c] / 32768.f;
            m_fifo.push_back(s / float(ch));
        }
        break;
    }
    case QAudioFormat::Int32: {
        const qint32* p = reinterpret_cast<const qint32*>(data);
        const int frames = bytes / (int(sizeof(qint32)) * ch);
        for (int i=0; i<frames; ++i) {
            float s = 0.f;
            for (int c=0; c<ch; ++c)
                s += p[i*ch + c] / 2147483648.0f; // 2^31
            m_fifo.push_back(s / float(ch));
        }
        break;
    }
    case QAudioFormat::Float: {
        const float* p = reinterpret_cast<const float*>(data);
        const int frames = bytes / (int(sizeof(float)) * ch);
        for (int i=0; i<frames; ++i) {
            float s = 0.f;
            for (int c=0; c<ch; ++c)
                s += p[i*ch + c];
            m_fifo.push_back(s / float(ch));
        }
        break;
    }
    case QAudioFormat::UInt8: { // 8-bit unsigned (raro, mas existe)
        const quint8* p = reinterpret_cast<const quint8*>(data);
        const int frames = bytes / (int(sizeof(quint8)) * ch);
        for (int i=0; i<frames; ++i) {
            float s = 0.f;
            for (int c=0; c<ch; ++c)
                s += (float(p[i*ch + c]) - 128.f) / 128.f; // 0..255 -> -1..1
            m_fifo.push_back(s / float(ch));
        }
        break;
    }
    default:
        // formato não suportado: ignora o chunk
        break;
    }

    const int maxKeep = std::max(m_sampleRate + m_analysisSize, m_sampleRate * 3 / 2);
    if (m_fifo.size() > maxKeep) {
        const int drop = m_fifo.size() - maxKeep;
        m_fifo.erase(m_fifo.begin(), m_fifo.begin() + drop);
    }
}

// ----------------- Análise -----------------
void PitchTracker::processAnalysis()
{
    if (m_fifo.size() < m_analysisSize) return;

    // janela mais recente
    const int N = m_analysisSize;
    const int start = m_fifo.size() - N;
    QVector<float> x(N);
    std::copy(m_fifo.constBegin() + start, m_fifo.constBegin() + start + N, x.begin());

    // remove DC + Hann
    double mean = 0.0;
    for (float v : x) mean += v;
    mean /= double(N);
    for (int i=0; i<N; ++i) {
        x[i] = float(x[i] - mean);
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (N - 1));
        x[i] *= float(w);
    }

    // silêncio?
    double rms = 0.0;
    for (float v : x) rms += double(v) * double(v);
    rms = std::sqrt(rms / double(N));
    if (rms < m_silenceThresh) {
        emit pitchFrequency(0.0, 0.0);
        emit noteUpdate(69, 0.0, 0.0, 0.0);
        return;
    }

    double conf = 0.0;
    const double f0 = detectPitchACF(x.constData(), N, m_sampleRate, m_minF, m_maxF, &conf);

    if (f0 > 0.0) {
        const int midi = freqToMidi(f0);
        double cents = centsDelta(f0, midi);
        if (cents <= -50.0) cents += 100.0;
        if (cents >   50.0) cents -= 100.0;

        emit pitchFrequency(f0, conf);
        emit noteUpdate(midi, cents, f0, conf);
    } else {
        emit pitchFrequency(0.0, 0.0);
        emit noteUpdate(69, 0.0, 0.0, 0.0);
    }
}

double PitchTracker::detectPitchACF(const float* x, int N, int sr,
                                    double minF, double maxF, double* confOut) const
{
    const int minLag = int(sr / std::max(20.0, maxF));   // lag pequeno (freq alta)
    const int maxLag = int(sr / std::max(1.0,  minF));   // lag grande  (freq baixa)
    if (maxLag + 1 >= N || minLag < 2) return 0.0;

    // autocorrelação
    QVector<double> R(maxLag + 1, 0.0);
    for (int k = 0; k <= maxLag; ++k) {
        double s = 0.0;
        const int M = N - k;
        const float* a = x;
        const float* b = x + k;
        for (int i=0; i<M; ++i) s += double(a[i]) * double(b[i]);
        R[k] = s;
    }

    const double R0 = std::max(1e-9, R[0]);

    // pico global em [minLag..maxLag]
    int bestLag = minLag;
    double bestVal = -1e12;
    for (int k = minLag; k <= maxLag - 1; ++k) {
        if (R[k] > bestVal) {
            bestVal = R[k];
            bestLag = k;
        }
    }

    // interpolação parabólica
    double lag = double(bestLag);
    if (bestLag > 1 && bestLag < maxLag) {
        double y1 = R[bestLag - 1];
        double y2 = R[bestLag];
        double y3 = R[bestLag + 1];
        double denom = (y1 - 2.0*y2 + y3);
        if (std::abs(denom) > 1e-12) {
            double delta = 0.5 * (y1 - y3) / denom; // [-1..1]
            lag += delta;
        }
    }

    // confiança: pico / R0 (clamp 0..1)
    double conf = std::max(0.0, std::min(1.0, bestVal / R0));
    if (confOut) *confOut = conf;

    const double f0 = double(sr) / lag;
    if (f0 < minF || f0 > maxF) return 0.0;
    return f0;
}

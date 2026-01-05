#include "tonegenerator.h"

#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QIODevice>
#include <QDebug>

#include <cmath>      // std::sin, std::pow
#include <algorithm>  // std::clamp

// ===================== SineStream (gerador) =========================
class ToneGenerator::SineStream : public QIODevice
{
public:
    explicit SineStream(ToneGenerator* host)
        : QIODevice(host), m_host(host)
    {
        open(QIODevice::ReadOnly);
    }

    void setSampleRate(int sr)
    {
        m_sr = qMax(8000, sr);
        m_rampSamples = qMax(1, m_sr / 200); // ~5ms
    }

    void gate(bool on)
    {
        m_targetAmp = on ? m_volume : 0.0f; // rumo ao volume atual
    }

    void setLogicalVolume(float vol01)
    {
        m_volume = qBound(0.0f, vol01, 1.0f);
        // se está “on”, rampa para o novo volume; se “off”, alvo fica 0
        m_targetAmp = (m_targetAmp > 0.0f) ? m_volume : 0.0f;
    }

    // ---- PULL MODE: diga ao sink que sempre há dados ----
    bool isSequential() const override { return true; }

    qint64 bytesAvailable() const override
    {
        // Stream infinito: sempre há dados para puxar
        return 4096 + QIODevice::bytesAvailable();
    }

    bool atEnd() const override { return false; }

protected:
    qint64 readData(char* data, qint64 maxlen) override
    {
        // garante múltiplo de 2 bytes (int16)
        const qint64 bytes = maxlen - (maxlen % qint64(sizeof(int16_t)));
        if (bytes <= 0) return 0;

        int16_t* out = reinterpret_cast<int16_t*>(data);
        const int samples = int(bytes / qint64(sizeof(int16_t)));

        constexpr double twoPi = 2.0 * 3.14159265358979323846;

        for (int i = 0; i < samples; ++i) {
            const double f = m_host->m_freqHz.load(std::memory_order_relaxed);
            const double inc = twoPi * f / double(m_sr);

            // rampa linear anti-click
            if (m_amp < m_targetAmp) {
                m_amp = qMin(m_targetAmp, m_amp + (1.0f / float(m_rampSamples)));
            } else if (m_amp > m_targetAmp) {
                m_amp = qMax(m_targetAmp, m_amp - (1.0f / float(m_rampSamples)));
            }

            const double s = std::sin(m_phase) * double(m_amp);
            m_phase += inc;
            if (m_phase >= twoPi) m_phase -= twoPi;

            const int sample = int(qBound(-1.0, s, 1.0) * 32767.0);
            out[i] = qint16(sample);
        }

        return bytes;
    }

    qint64 writeData(const char*, qint64) override { return -1; }

private:
    ToneGenerator* m_host = nullptr;
    int   m_sr = 44100;
    int   m_rampSamples = 220;   // ~5ms @44.1k
    float m_amp = 0.0f;
    float m_targetAmp = 0.0f;
    float m_volume = 0.85f;      // volume lógico
    double m_phase = 0.0;
};

// ===================== ToneGenerator ===============================
ToneGenerator::ToneGenerator(QObject* parent)
    : QObject(parent)
{
    m_sine = new SineStream(this);
    m_stream = m_sine; // pull mode
}

ToneGenerator::~ToneGenerator()
{
    stop();

    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    if (m_sine) {
        m_sine->close();
        delete m_sine;
        m_sine = nullptr;
    }
    m_stream = nullptr;
}

// --------------------- API pública ---------------------------------
void ToneGenerator::start()
{
    ensureAudio();
    if (!m_sink || !m_sine) return;

    updateFrequency();
    updateLabel();

    // abre gate antes de iniciar
    m_sine->gate(true);

    // (re)start do sink se necessário
    if (m_sink->state() != QAudio::ActiveState) {
        m_sink->start(m_sine);

        // log útil para Android (pode remover depois)
        qDebug() << "[ToneSink] after start state=" << m_sink->state()
                 << "error=" << m_sink->error();
    }

    if (!m_playing) {
        m_playing = true;
        emit started();
    }
}

void ToneGenerator::stop()
{
    if (m_sine) m_sine->gate(false);

    // libera a saída (importantíssimo no Android para coexistir com metrônomo)
    if (m_sink && m_sink->state() != QAudio::StoppedState) {
        m_sink->stop();
    }

    if (m_playing) {
        m_playing = false;
        emit stopped();
    }
}

void ToneGenerator::setNoteIndex(int idx)
{
    idx = qBound(0, idx, 6);
    if (m_noteIndex == idx) return;
    m_noteIndex = idx;
    updateFrequency();
    updateLabel();
}

void ToneGenerator::setNoteName(const QString& name)
{
    if (name.isEmpty()) return;
    const QChar c = name.at(0).toUpper();
    static const QString order = "CDEFGAB";
    const int idx = order.indexOf(c);
    if (idx >= 0) setNoteIndex(idx);
}

void ToneGenerator::setAccidental(Accidental a)
{
    if (m_acc == a) return;
    m_acc = a;
    updateFrequency();
    updateLabel();
}

void ToneGenerator::setOctave(int oct)
{
    oct = qBound(m_minOctave, oct, m_maxOctave);
    if (m_octave == oct) return;
    m_octave = oct;
    updateFrequency();
    updateLabel();
}

void ToneGenerator::octaveUp()   { setOctave(m_octave + 1); }
void ToneGenerator::octaveDown() { setOctave(m_octave - 1); }

void ToneGenerator::setVolume(float vol01)
{
    m_volume = qBound(0.0f, vol01, 1.0f);
    if (m_sine) m_sine->setLogicalVolume(m_volume);
}

// --------------------- helpers -------------------------------------
QString ToneGenerator::noteLabel() const
{
    static const char* names[7] = {"DO","RE","MI","FA","SOL","LA","SI"};
    QString lab = names[m_noteIndex];
    if      (m_acc == Sharp) lab += "#";
    else if (m_acc == Flat)  lab += "b";
    lab += QString::number(m_octave > 0 ? m_octave - 1 : m_octave);
    return lab;
}

void ToneGenerator::updateLabel()
{
    emit noteLabelChanged(noteLabel());
}

int ToneGenerator::semitoneForNoteIndex(int idx)
{
    static const int map[7] = {0, 2, 4, 5, 7, 9, 11};
    return map[qBound(0, idx, 6)];
}

int ToneGenerator::midiFromNote(int noteIdx, Accidental acc, int octave)
{
    int semi = semitoneForNoteIndex(noteIdx) + int(acc);
    int oct  = octave;
    if (semi >= 12) { semi -= 12; ++oct; }
    if (semi < 0)   { semi += 12; --oct; }
    return (oct + 1) * 12 + semi;
}

double ToneGenerator::freqFromMidi(int midi)
{
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

void ToneGenerator::updateFrequency()
{
    const int midi = midiFromNote(m_noteIndex, m_acc, m_octave);
    double hz = freqFromMidi(midi);

    // clamps suaves para speaker + Nyquist
    hz = qBound(80.0, hz, qMin(6000.0, 0.45 * m_sampleRate));

    m_freqHz.store(hz, std::memory_order_relaxed);
    emit frequencyChanged(hz);
}

void ToneGenerator::setTargetAmplitude(float a)
{
    if (m_sine) m_sine->setLogicalVolume(qBound(0.0f, a, 1.0f));
}

void ToneGenerator::ensureAudio()
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

    m_fmt = fmt;
    m_sampleRate = m_fmt.sampleRate();

    // limites de oitava baseados no sample rate
    m_minOctave = 1;
    m_maxOctave = (m_sampleRate < 32000) ? 6 : 7;

    m_sink = new QAudioSink(dev, m_fmt, this);
    m_sink->setVolume(1.0f);

    // prepara o gerador
    if (m_sine) {
        m_sine->setSampleRate(m_sampleRate);
        m_sine->setLogicalVolume(m_volume);
    }

    // log útil (remova depois)
    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State st){
        qDebug() << "[ToneSink] state=" << st << "error=" << (m_sink ? m_sink->error() : QAudio::NoError);
    });
}

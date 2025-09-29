#include "tonegenerator.h"

#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QtMath>
#include <QDebug>
#include <QIODevice>

// ===================== SineStream (gerador) =========================
class ToneGenerator::SineStream : public QIODevice
{
public:
    explicit SineStream(ToneGenerator* host)
        : QIODevice(host), m_host(host)
    {
        open(QIODevice::ReadOnly);
    }

    void setSampleRate(int sr) {
        m_sr = qMax(8000, sr);
        m_rampSamples = qMax(1, m_sr / 200); // ~5ms
    }

    void setVolume(float vol01) {
        m_targetAmp = qBound(0.0f, vol01, 1.0f);
    }

    void gate(bool on) {
        m_targetAmp = on ? m_volume : 0.0f; // rumo ao volume atual
    }

    void setLogicalVolume(float vol01) {
        m_volume = qBound(0.0f, vol01, 1.0f);
        // se já estou "on", quero ir até o novo volume; se "off", alvo é 0
        m_targetAmp = (m_targetAmp > 0.0f) ? m_volume : 0.0f;
    }

protected:
    qint64 readData(char* data, qint64 maxlen) override
    {
        // 16-bit mono
        const int16_t* end = reinterpret_cast<const int16_t*>(data + maxlen);
        int16_t* out = reinterpret_cast<int16_t*>(data);

        const double twoPi = 2.0 * M_PI;

        while (reinterpret_cast<char*>(out) < reinterpret_cast<const char*>(end)) {
            const double f = m_host->m_freqHz.load(std::memory_order_relaxed);
            const double inc = twoPi * f / double(m_sr);

            // Rampa de amplitude linear
            if (m_amp < m_targetAmp) {
                m_amp = qMin(m_targetAmp, m_amp + (1.0f / m_rampSamples));
            } else if (m_amp > m_targetAmp) {
                m_amp = qMax(m_targetAmp, m_amp - (1.0f / m_rampSamples));
            }

            // Seno
            const double s = std::sin(m_phase) * double(m_amp);
            m_phase += inc;
            if (m_phase >= twoPi) m_phase -= twoPi;

            const int sample = int(qBound(-1.0, s, 1.0) * 32767.0);
            *out++ = qint16(sample);
        }
        return maxlen;
    }

    qint64 writeData(const char*, qint64) override { return -1; }

private:
    ToneGenerator* m_host;
    int   m_sr = 44100;
    int   m_rampSamples = 220;   // ~5ms @44.1k
    float m_amp = 0.0f;
    float m_targetAmp = 0.0f;
    float m_volume = 0.85f;      // volume “lógico” (alvo da rampa ao ligar)
    double m_phase = 0.0;
};

// ===================== ToneGenerator ===============================
ToneGenerator::ToneGenerator(QObject* parent)
    : QObject(parent)
{
    // cria o gerador; a sink é criada em ensureAudio()
    m_sine = new SineStream(this);
}

ToneGenerator::~ToneGenerator()
{
    stop();
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
    if (m_sine) { m_sine->close(); delete m_sine; m_sine = nullptr; }
}

// --------------------- API pública ---------------------------------
void ToneGenerator::start()
{
    ensureAudio();                // cria sink e ajusta formato
    if (!m_sink) return;

    // define frequência atual e abre o gate
    updateFrequency();
    updateLabel();
    m_sine->gate(true);
    m_playing = true;
    emit started();
}

void ToneGenerator::stop()
{
    if (!m_playing) {
        // ainda assim feche o gate (silêncio suave)
        if (m_sine) m_sine->gate(false);
        return;
    }
    if (m_sine) m_sine->gate(false);
    m_playing = false;
    emit stopped();
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
    // limita pelo dispositivo (Nyquist e audibilidade ≈ 60..6000 Hz)
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
    static const char* names[7] = {"C","D","E","F","G","A","B"};
    QString lab = names[m_noteIndex];
    if      (m_acc == Sharp) lab += "#";
    else if (m_acc == Flat)  lab += "b";
    lab += QString::number(m_octave);
    return lab;
}

void ToneGenerator::updateLabel()
{
    emit noteLabelChanged(noteLabel());
}

int ToneGenerator::semitoneForNoteIndex(int idx)
{
    // C, D, E, F, G, A, B
    static const int map[7] = {0, 2, 4, 5, 7, 9, 11};
    return map[qBound(0, idx, 6)];
}

int ToneGenerator::midiFromNote(int noteIdx, Accidental acc, int octave)
{
    // MIDI: C-1 = 0 => midi = (octave+1)*12 + semitone
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
    // calcula MIDI -> Hz
    const int midi = midiFromNote(m_noteIndex, m_acc, m_octave);
    double hz = freqFromMidi(midi);

    // Clamps suaves para speaker de celular (~80..6000 Hz)
    hz = qBound(80.0, hz, qMin(6000.0, 0.45 * m_sampleRate)); // respeita Nyquist

    m_freqHz.store(hz, std::memory_order_relaxed);
    emit frequencyChanged(hz);

    // Se já está tocando, não precisa reiniciar a sink—o gerador usa a nova f instantaneamente
}

void ToneGenerator::setTargetAmplitude(float a)
{
    if (m_sine) {
        m_sine->setLogicalVolume(qBound(0.0f, a, 1.0f));
    }
}

void ToneGenerator::ensureAudio()
{
    if (m_sink && m_stream) return;

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

    // Ajusta limites de oitava baseado no sample rate
    // (C2 ≈ 65 Hz, C7 ≈ 2093 Hz, C8 ≈ 4186 Hz)
    m_minOctave = 1; // C1.. (≈ 32Hz) — colocamos 1 para evitar subgrave
    m_maxOctave = 7;
    if (m_sampleRate < 32000) m_maxOctave = 6;

    // (re)cria a sink
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
    m_sink = new QAudioSink(dev, m_fmt, this);
    m_sink->setVolume(1.0f);

    m_sine->setSampleRate(m_sampleRate);
    m_sine->setVolume(m_volume);

    // PULL MODE: a sink puxa dados do seu QIODevice gerador
    m_sink->start(m_sine);
    m_stream = nullptr; // não usado em pull mode
}

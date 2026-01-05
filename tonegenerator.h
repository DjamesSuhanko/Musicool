#pragma once

#include <QObject>
#include <QAudioFormat>
#include <QString>
#include <atomic>

class QAudioSink;
class QIODevice;

class ToneGenerator : public QObject
{
    Q_OBJECT
public:
    enum Accidental { Natural = 0, Sharp = 1, Flat = -1 };
    Q_ENUM(Accidental)

    explicit ToneGenerator(QObject* parent = nullptr);
    ~ToneGenerator() override;

    // Controle principal
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool isPlaying() const { return m_playing; }

    // Nota base (0..6) => DO, RE, MI, FA, SOL, LA, SI
    void setNoteIndex(int idx);
    int  noteIndex() const { return m_noteIndex; }

    // Alternativa: por texto "C","D","E","F","G","A","B"
    void setNoteName(const QString& name);

    // Acidente
    void setAccidental(Accidental a);
    Accidental accidental() const { return m_acc; }

    // Oitava (MIDI: C4=60 ⇒ aqui octave=4)
    void setOctave(int oct);
    int  octave() const { return m_octave; }
    void octaveUp();
    void octaveDown();

    // Volume 0..1
    void setVolume(float vol01);
    float volume() const { return m_volume; }

    // Informações atuais
    double frequency() const { return m_freqHz.load(std::memory_order_relaxed); }
    QString noteLabel() const; // mantém seu padrão "DO/RE/..." com #/b e oitava

signals:
    void frequencyChanged(double hz);
    void noteLabelChanged(const QString& label);
    void started();
    void stopped();

private:
    void ensureAudio();          // cria/ajusta QAudioSink
    void updateFrequency();      // recalcula freq e emite frequencyChanged
    void updateLabel();          // emite noteLabelChanged
    void setTargetAmplitude(float a);

    static int    semitoneForNoteIndex(int idx);
    static int    midiFromNote(int noteIdx, Accidental acc, int octave);
    static double freqFromMidi(int midi);

private:
    // Estado musical
    int        m_noteIndex  = 0;
    Accidental m_acc        = Natural;
    int        m_octave     = 4;
    float      m_volume     = 0.85f;

    // Limites por dispositivo
    int        m_minOctave  = 0;
    int        m_maxOctave  = 8;

    // Áudio
    QAudioSink*  m_sink      = nullptr;
    QIODevice*   m_stream    = nullptr; // em pull mode, apontará para m_sine
    QAudioFormat m_fmt;
    int          m_sampleRate = 44100;

    // Execução
    std::atomic<double> m_freqHz { 0.0 };
    bool m_playing = false;

    class SineStream;
    SineStream* m_sine = nullptr;
};

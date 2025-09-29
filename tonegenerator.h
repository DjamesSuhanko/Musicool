#pragma once
#include <QObject>
#include <QAudioFormat>
#include <QVector>
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
    Q_INVOKABLE void start();           // começa a tocar a nota atual
    Q_INVOKABLE void stop();            // para de tocar
    Q_INVOKABLE bool isPlaying() const { return m_playing; }

    // Nota base (0..6) => C, D, E, F, G, A, B
    void setNoteIndex(int idx);         // 0=C,1=D,...,6=B
    int  noteIndex() const { return m_noteIndex; }

    // Alternativa: por texto "C","D","E","F","G","A","B"
    void setNoteName(const QString& name);

    // Acidente
    void setAccidental(Accidental a);
    Accidental accidental() const { return m_acc; }

    // Oitava (MIDI: C4=60 ⇒ aqui octave=4)
    void setOctave(int oct);            // clamped ao range suportado
    int  octave() const { return m_octave; }
    void octaveUp();
    void octaveDown();

    // Volume 0..1
    void setVolume(float vol01);
    float volume() const { return m_volume; }

    // Informações da nota/frequência atual
    double frequency() const { return m_freqHz; }
    QString noteLabel() const;          // "C#4" ou "Db4" conforme o acidente setado

signals:
    void frequencyChanged(double hz);
    void noteLabelChanged(const QString& label);
    void started();
    void stopped();

private:
    // Áudio
    void ensureAudio();              // cria/ajusta QAudioSink
    void updateFrequency();          // recalcula freq e envia ao gerador
    void updateLabel();              // emite rótulo da nota
    void setTargetAmplitude(float a);// rampa de amplitude

private:
    // Mapeamentos
    static int  semitoneForNoteIndex(int idx); // C=0,D=2,E=4,F=5,G=7,A=9,B=11
    static int  midiFromNote(int noteIdx, Accidental acc, int octave); // MIDI
    static double freqFromMidi(int midi); // A4=440

private:
    // Estado musical
    int         m_noteIndex  = 0;       // 0..6 => C..B
    Accidental  m_acc        = Natural; // ♮,♯,♭
    int         m_octave     = 4;       // padrão A4 próximo
    float       m_volume     = 0.85f;   // 0..1

    // Limites (ajustados pelo dispositivo)
    int         m_minOctave  = 0;
    int         m_maxOctave  = 8;

    // Áudio
    QAudioSink* m_sink       = nullptr;
    QIODevice*  m_stream     = nullptr; // gerador (QIODevice) usado pela sink
    QAudioFormat m_fmt;
    int         m_sampleRate = 44100;

    // Sinal senoidal (estado de execução)
    std::atomic<double> m_freqHz {0.0};
    bool        m_playing   = false;

    // --- implementação do gerador (classe interna) ---
    class SineStream;
    SineStream* m_sine = nullptr;
};

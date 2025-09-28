#pragma once

#include <QObject>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>
#include <QVector>
#include <QElapsedTimer>

class PitchTracker : public QObject
{
    Q_OBJECT
public:
    explicit PitchTracker(QObject* parent = nullptr);
    ~PitchTracker();

    // Configurações básicas (chame antes de start(), se quiser alterar)
    void setMinFrequency(double hz);     // default: 60 Hz
    void setMaxFrequency(double hz);     // default: 1200 Hz
    void setAnalysisSize(int samples);   // default: 4096
    void setProcessIntervalMs(int ms);   // throttling; default: ~40 ms
    void setSilenceRmsThreshold(double t); // 0..1 (escala float), default: 0.005

public slots:
    bool start();   // inicia microfone; retorna false se falhar
    void stop();    // para microfone

signals:
    void started();
    void stopped();

    // Emite frequência detectada (Hz) e confiança [0..1] (0=ruim, 1=ótimo)
    void pitchFrequency(double hz, double confidence);

    // Emite nota + cents relativos à nota mais próxima ([-50,+50]) + Hz + confiança
    void noteUpdate(int midi, double cents, double hz, double confidence);

private slots:
    void onReadyRead();

private:
    // Conversão de PCM para float e enfileiramento
    void pushSamplesFromBytes(const char* data, int bytes);

    // Processa último bloco (analysisSize) e emite sinais
    void processAnalysis();

    // Detecção de pitch por autocorrelação (com interp. parabólica)
    // Retorna Hz; *conf retorna medida simples de confiança (pico/R0)
    double detectPitchACF(const float* x, int N, int sr,
                          double minF, double maxF, double* confOut) const;

    static inline int freqToMidi(double f) {
        if (f <= 0.0) return 69;
        return int(std::lround(69.0 + 12.0 * std::log2(f / 440.0)));
    }
    static inline double midiToFreq(int midi) {
        return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    }
    static inline double centsDelta(double f, int midi) {
        const double fref = midiToFreq(midi);
        return 1200.0 * std::log2(f / fref);
    }

private:
    // Áudio
    QAudioSource*  m_source = nullptr;
    QIODevice*     m_io     = nullptr;
    QAudioFormat   m_fmt;

    // Buffer FIFO de áudio em float mono
    QVector<float> m_fifo;

    // Parâmetros
    int     m_sampleRate       = 48000;
    int     m_channels         = 1;
    int     m_analysisSize     = 4096;
    double  m_minF             = 60.0;
    double  m_maxF             = 1200.0;
    int     m_processInterval  = 40;     // ms
    double  m_silenceThresh    = 0.005;  // RMS (float 0..1)

    // Controle
    QElapsedTimer m_timer;
    bool    m_running = false;
};

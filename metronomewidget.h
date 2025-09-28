#pragma once
#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QColor>

class QAudioSink;   // Qt 6
class QIODevice;

class MetronomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MetronomeWidget(QWidget *parent = nullptr);
    ~MetronomeWidget() override;

    // Parâmetros de controle
    void setBeatsPerMeasure(int beats); // aceita 2, 3 ou 4
    void setBpm(int bpm);               // 30..300
    void setRunning(bool on);           // start/stop visual + som
    void setAudioEnabled(bool on);
    void setAccentEnabled(bool on);
    void setVolume(float vol01);        // 0..1
    void setDownbeatHz(double hz);      // frequência do 1º tempo
    void setUpbeatHz(double hz);        // frequência dos demais

    // Estado
    int  beatsPerMeasure() const { return m_beats; }
    int  bpm() const             { return m_bpm; }
    bool isRunning() const       { return m_running; }

signals:
    // Notifica a batida (0..beats-1), útil se quiser sincronizar algo externo
    void tick(int beatIndex, bool isDownbeat);

public slots:
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {560, 160}; }

private slots:
    void onBeat();

private:
    // desenho
    void drawBackground(QPainter &g);
    void drawBeatSquares(QPainter &g);

    // áudio
    void ensureAudio();
    void prepareClicks();                // (re)gera samples p/ o formato atual
    void playClick(bool downbeat);

private:
    // parâmetros
    int   m_beats       = 4;    // 2,3,4
    int   m_bpm         = 120;  // 30..300
    bool  m_running     = false;
    bool  m_audioOn     = true;
    bool  m_accentOn    = true;
    float m_volume      = 0.85f;
    double m_fDownbeat  = 500.0; // Hz (grave)
    double m_fUpbeat    = 900.0; // Hz

    // estado
    int   m_currentBeat = 0;     // 0..m_beats-1
    QTimer m_timer;

    // áudio (Qt Multimedia)
    QAudioSink* m_sink  = nullptr;
    QIODevice*  m_out   = nullptr;
    int         m_sampleRate = 44100;
    QVector<qint16> m_clickDown;  // samples do click do 1º tempo
    QVector<qint16> m_clickUp;    // samples dos demais tempos

    // paleta (dark)
    QColor m_bg          = QColor("#121212");
    QColor m_box         = QColor("#26262A");
    QColor m_boxBorder   = QColor("#3C3C40");
    QColor m_text        = QColor("#EEEEEE");
    QColor m_highlightUp = QColor("#4F8AFF");  // destaque normal
    QColor m_highlightDn = QColor("#22B14C");  // destaque do 1º tempo (verde)
};

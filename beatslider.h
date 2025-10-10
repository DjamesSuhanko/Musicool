#pragma once
#include <QWidget>
#include <QColor>
#include <QVector>
//#include <Qt> // garante Qt::Orientation


class BeatSlider : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    explicit BeatSlider(QWidget* parent = nullptr);

    // valores discretos (padrão: {2,3,4})
    void setDiscreteValues(const QVector<int>& vals);
    QVector<int> discreteValues() const { return m_vals; }

    // valor atual (deve pertencer a m_vals)
    int  value() const { return m_value; }
    void setValue(int v);            // com sinal
    void setValueSilently(int v);    // sem sinal

    // estilo
    void setColors(QColor track, QColor active, QColor tick, QColor handle, QColor text);
    void setShowLabels(bool on) { m_showLabels = on; update(); }
    bool showLabels() const { return m_showLabels; }
    // Compat: o BeatSlider é sempre horizontal, mas expomos a API:
    void setOrientation(Qt::Orientation) {}            // no-op
    Qt::Orientation orientation() const { return Qt::Horizontal; }

    // Ajustes de compacidade
    void setTrackThickness(int px);      // espessura do trilho (mín 4)
    void setHandleRadius(int px);        // raio do knob (mín 10)
    void setHorizontalPadding(int px);   // padding lateral
    void setVerticalPadding(int px);     // padding sup/inf

    // Preset rápido para ficar baixinho
    void setCompactPresetSmall() {
        setTrackThickness(6);
        setHandleRadius(12);
        setVerticalPadding(4);
    }

signals:
    void valueChanged(int v);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    QSize sizeHint() const override { return { 420, 84 }; }
    QSize minimumSizeHint() const override { return { 240, 64 }; }

private:
    // geom helpers
    QRectF trackRect() const;
    QVector<QPointF> tickPoints() const;
    int nearestIndexAtX(qreal x) const;

private:
    QVector<int> m_vals {2,3,4};
    int   m_value = 4;
    int   m_dragIndex = -1;
    bool  m_dragging = false;

    // Cores (padrão: verde escuro; sem azul)
    QColor m_track   = QColor("#0B3D0B");  // trilho
    QColor m_active  = QColor("#0B3D0B");  // parte “ativa” (mesma cor p/ não ficar azul)
    QColor m_tick    = QColor("#B0B0B0");
    QColor m_handle  = QColor("#A8FF00");  // knob
    QColor m_text    = QColor("#E0E0E0");

    // Tamanhos (compactação)
    int   m_trackThickness = 8;   // px
    int   m_handleRadius   = 18;  // px
    int   m_padX           = 18;  // px
    int   m_padY           = 10;  // px

    bool m_showLabels = true;

    // cache simples
    QVector<QPointF> m_tickPts;
};

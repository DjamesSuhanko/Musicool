#ifndef COMPASSCALCULATOR_H
#define COMPASSCALCULATOR_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QButtonGroup>
#include <QAbstractButton>
#include <cstdint>

struct Frac {
    int64_t num = 0; // fração da semibreve: seminima=1/4, colcheia=1/8 etc.
    int64_t den = 1;

    Frac() = default;
    Frac(int64_t n, int64_t d) : num(n), den(d) { normalize(); }

    static int64_t igcd(int64_t a, int64_t b) {
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        while (b) { int64_t t = a % b; a = b; b = t; }
        return a ? a : 1;
    }
    void normalize() {
        if (den < 0) { den = -den; num = -num; }
        if (num == 0) { den = 1; return; }
        int64_t g = igcd(num, den);
        num /= g; den /= g;
    }
    Frac& operator+=(const Frac& o) {
        int64_t n = num * o.den + o.num * den;
        int64_t d = den * o.den;
        num = n; den = d; normalize();
        return *this;
    }
    Frac operator-() const { return Frac{-num, den}; }
};

class CompassCalculator : public QObject {
    Q_OBJECT
public:
    explicit CompassCalculator(QObject* parent = nullptr);

    // IDs de botões para usar no seu QButtonGroup
    enum ButtonId {
        // Notas
        BTN_NOTE_SEMIBREVE = 1,  // 1/1 da semibreve
        BTN_NOTE_MINIMA,         // 1/2
        BTN_NOTE_SEMINIMA,       // 1/4
        BTN_NOTE_COLCHEIA,       // 1/8
        BTN_NOTE_SEMICOLCHEIA,   // 1/16

        // Pausas
        BTN_REST_SEMIBREVE,
        BTN_REST_MINIMA,
        BTN_REST_SEMINIMA,
        BTN_REST_COLCHEIA,
        BTN_REST_SEMICOLCHEIA,

        // Edição
        BTN_DOT,        // pontua a última figura base ainda não pontuada
        BTN_BACKSPACE,
        BTN_CLEAR
    };
    Q_ENUM(ButtonId)

    void setButtonGroup(QButtonGroup* group);

    // Consulta de estado
    Frac total() const { return m_total; }                 // fração da semibreve
    QString totalText() const;                              // p.ex. "3/4", "1"
    QString sequenceText() const;                           // ex.: "♪ + 𝄾 + . + ♩"
    QString signatureText() const;                          // "N/D" (compasso)
    QString signatureStackedHtml() const;                   // HTML empilhado para QLabel
    int signatureNumerator()   const { return m_sigNum; }
    int signatureDenominator() const { return m_sigDen; }   // prefere 4,2,1,8,16,32

signals:
    void totalChanged(const Frac& total, const QString& totalText);
    void sequenceChanged(const QString& sequence);
    void signatureChanged(int numerator, int denominator,
                          const QString& textND, const QString& stackedHtml);

public slots:
    void handleButtonId(int id);
    void handleButton(QAbstractButton* btn); // alternativa via propriedades

    void addNote(bool isRest, int baseRef); // baseRef ∈ {1,2,4,8,16} (denominador relativo à semibreve)
    void addDot();                           // pontua a última figura base
    void backspace();
    void clear();

private:
    struct Entry {
        bool isAug = false;   // true se esta entry é o "ponto" (aumento) de uma base
        bool isRest = false;  // válido somente quando !isAug
        int  refDen = 1;      // denominador da figura base (1,2,4,8,16) ou do ponto (2*refDen)
        bool hasDot = false;  // marca na figura base se já recebeu ponto
        QString label;        // "♩", "♪", "𝄽", "."
        Frac value;           // valor em fração da semibreve
    };

    // utilidades
    static Frac   fracFromRef(int refDen);               // 1/refDen
    static QString glyphFromRef(bool isRest, int refDen);

    void push(const Entry& e);
    void recomputeSignature();

    // escolha do denominador-alvo preferindo seminima (4), depois 2,1 e só então 8,16,32
    int chooseSignatureDenPreferred() const; // retorna {4,2,1,8,16,32} que divide exatamente o total

    // estado
    QVector<Entry> m_stack;
    Frac m_total;
    int m_sigNum = 0;
    int m_sigDen = 4;
};

#endif // COMPASSCALCULATOR_H

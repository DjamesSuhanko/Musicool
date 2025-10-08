#include "CompassCalculator.h"
#include <QStringList>
#include <QtMath>

static inline Frac make_frac(int64_t n, int64_t d) { return Frac{n,d}; }

CompassCalculator::CompassCalculator(QObject* parent)
    : QObject(parent)
{
    m_total = Frac{0,1};
    m_sigNum = 0; m_sigDen = 4;
}

void CompassCalculator::setButtonGroup(QButtonGroup* group) {
    if (!group) return;
    connect(group, SIGNAL(buttonClicked(int)), this, SLOT(handleButtonId(int)));
    connect(group, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(handleButton(QAbstractButton*)));
}

QString CompassCalculator::totalText() const {
    if (m_total.den == 1) return QString::number(m_total.num);
    return QString("%1/%2").arg(m_total.num).arg(m_total.den);
}

QString CompassCalculator::sequenceText() const {
    QStringList parts;
    parts.reserve(m_stack.size());
    for (const auto& e : m_stack) parts << e.label;
    return parts.join(" + ");
}

QString CompassCalculator::signatureText() const {
    return QString("%1/%2").arg(m_sigNum).arg(m_sigDen);
}

QString CompassCalculator::signatureStackedHtml() const {
    return QString(
               "<table cellspacing='0' cellpadding='0' style='border-collapse:collapse;"
               "font-size:200%%; line-height:1; text-align:center;'>"
               "<tr><td style='padding:0;'>%1</td></tr>"
               "<tr><td style='padding:0;'><div style='border-top:2px solid; width:100%%;'></div></td></tr>"
               "<tr><td style='padding:0;'>%2</td></tr>"
               "</table>").arg(m_sigNum).arg(m_sigDen);
}

void CompassCalculator::handleButtonId(int id) {
    switch (id) {
    // Notas
    case BTN_NOTE_SEMIBREVE:      addNote(false, 1);  break;
    case BTN_NOTE_MINIMA:         addNote(false, 2);  break;
    case BTN_NOTE_SEMINIMA:       addNote(false, 4);  break;
    case BTN_NOTE_COLCHEIA:       addNote(false, 8);  break;
    case BTN_NOTE_SEMICOLCHEIA:   addNote(false,16);  break;

    // Pausas
    case BTN_REST_SEMIBREVE:      addNote(true, 1);   break;
    case BTN_REST_MINIMA:         addNote(true, 2);   break;
    case BTN_REST_SEMINIMA:       addNote(true, 4);   break;
    case BTN_REST_COLCHEIA:       addNote(true, 8);   break;
    case BTN_REST_SEMICOLCHEIA:   addNote(true,16);   break;

    // Edição
    case BTN_DOT:        addDot();     break;
    case BTN_BACKSPACE:  backspace();  break;
    case BTN_CLEAR:      clear();      break;
    default: break;
    }
}

void CompassCalculator::handleButton(QAbstractButton* btn) {
    if (!btn) return;
    if (btn->property("dot").toBool()) { addDot(); return; }

    bool ok=false;
    int ref = btn->property("ref").toInt(&ok);
    if (!ok || (ref!=1 && ref!=2 && ref!=4 && ref!=8 && ref!=16)) return;
    bool isRest = btn->property("isRest").toBool();
    addNote(isRest, ref);
}

Frac CompassCalculator::fracFromRef(int refDen) {
    // valor da figura em fração da semibreve: 1/refDen
    return make_frac(1, refDen);
}

QString CompassCalculator::glyphFromRef(bool isRest, int refDen) {
    if (isRest) {
        switch (refDen) {
        case 1:  return QString::fromUtf8("𝄻"); // pausa semibreve
        case 2:  return QString::fromUtf8("𝄼"); // pausa mínima (aprox.)
        case 4:  return QString::fromUtf8("𝄽"); // pausa seminima
        case 8:  return QString::fromUtf8("𝄾"); // pausa colcheia
        case 16: return QString::fromUtf8("𝄿"); // pausa semicolcheia
        default: return "rest";
        }
    } else {
        switch (refDen) {
        case 1:  return QString::fromUtf8("𝅝"); // semibreve
        case 2:  return QString::fromUtf8("𝅗𝅥"); // mínima
        case 4:  return QString::fromUtf8("♩"); // seminima
        case 8:  return QString::fromUtf8("♪"); // colcheia
        case 16: return QString::fromUtf8("𝅘𝅥𝅯"); // semicolcheia
        default: return "note";
        }
    }
}

void CompassCalculator::push(const Entry& e) {
    m_stack.push_back(e);
    m_total += e.value;
    emit totalChanged(m_total, totalText());
    emit sequenceChanged(sequenceText());
    recomputeSignature();
}

void CompassCalculator::addNote(bool isRest, int baseRef) {
    Entry e;
    e.isAug = false;
    e.isRest = isRest;
    e.refDen = baseRef;          // 1,2,4,8,16
    e.hasDot = false;
    e.label = glyphFromRef(isRest, baseRef);
    e.value = fracFromRef(baseRef);
    push(e);
}

void CompassCalculator::addDot() {
    // procura a última figura base não pontuada
    for (int i = m_stack.size()-1; i >= 0; --i) {
        if (!m_stack[i].isAug) {
            if (m_stack[i].hasDot) return; // já pontuada
            // cria aumento = metade da base
            Entry aug;
            aug.isAug = true;
            aug.isRest = false;
            aug.refDen = m_stack[i].refDen * 2; // metade → 1/(2*ref)
            aug.hasDot = false;
            aug.label = ".";

            Frac base = fracFromRef(m_stack[i].refDen); // 1/ref
            aug.value = make_frac(base.num, base.den * 2); // (1/ref)/2

            m_stack[i].hasDot = true;
            push(aug);
            return;
        }
    }
    // sem base → ignorar
}

void CompassCalculator::backspace() {
    if (m_stack.isEmpty()) return;
    Entry last = m_stack.takeLast();
    m_total += -last.value;

    if (last.isAug) {
        // liberar a marcação de "ponto" da base correspondente mais próxima
        for (int i = m_stack.size()-1; i >= 0; --i) {
            if (!m_stack[i].isAug && m_stack[i].hasDot) { m_stack[i].hasDot = false; break; }
        }
    }

    emit totalChanged(m_total, totalText());
    emit sequenceChanged(sequenceText());
    recomputeSignature();
}

void CompassCalculator::clear() {
    m_stack.clear();
    m_total = Frac{0,1};
    m_sigNum = 0; m_sigDen = 4;
    emit totalChanged(m_total, totalText());
    emit sequenceChanged(sequenceText());
    emit signatureChanged(m_sigNum, m_sigDen, signatureText(), signatureStackedHtml());
}

// Escolha do denominador preferindo 4 (seminima), depois 2,1, e só então 8,16,32
int CompassCalculator::chooseSignatureDenPreferred() const {
    if (m_total.num == 0) return 4; // exibir "0/4"

    static const int prefs[] = {4, 2, 1, 8, 16, 32};
    for (int D : prefs) {
        // checa se (total * D) é inteiro
        const int64_t N_num = m_total.num * D;
        const int64_t N_den = m_total.den;
        if (N_num % N_den == 0) return D;
    }
    // fallback (não deve ocorrer com figuras binárias)
    return 4;
}

void CompassCalculator::recomputeSignature() {
    int D = chooseSignatureDenPreferred();
    int64_t N = (m_total.num * D) / m_total.den; // é inteiro pelo critério acima

    // ajuste preferido: um compasso inteiro deve ser 4/4 (não 1/1)
    if (m_total.den == 1 && m_total.num == 1) {
        D = 4; N = 4;
    }

    m_sigNum = static_cast<int>(N);
    m_sigDen = D;

    emit signatureChanged(m_sigNum, m_sigDen, signatureText(), signatureStackedHtml());
}

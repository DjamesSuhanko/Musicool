// theme.h
#pragma once
#include <QPalette>
#include <QColor>
#include <QString>

namespace Theme {
inline QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window,            QColor("#121212"));
    p.setColor(QPalette::WindowText,        QColor("#EEEEEE"));
    p.setColor(QPalette::Base,              QColor("#1A1B1E"));
    p.setColor(QPalette::AlternateBase,     QColor("#1F2125"));
    p.setColor(QPalette::Text,              QColor("#EEEEEE"));
    p.setColor(QPalette::Button,            QColor("#2B2D31"));
    p.setColor(QPalette::ButtonText,        QColor("#EEEEEE"));
    p.setColor(QPalette::BrightText,        QColor("#FFFFFF"));
    p.setColor(QPalette::ToolTipBase,       QColor("#EEEEEE"));
    p.setColor(QPalette::ToolTipText,       QColor("#121212"));
    p.setColor(QPalette::PlaceholderText,   QColor(0xEE,0xEE,0xEE,160));
    p.setColor(QPalette::Highlight,         QColor("#4F8AFF"));
    p.setColor(QPalette::HighlightedText,   QColor("#FFFFFF"));
    return p;
}

inline QString globalQss() {
    return QString::fromLatin1(R"(
      QWidget { background: #121212; }
      * { color: #EEEEEE; }
      QLineEdit, QTextEdit, QTextBrowser, QPlainTextEdit {
        background: #1A1B1E;
        selection-background-color: #4F8AFF;
        selection-color: #FFFFFF;
      }
      QToolTip { color: #121212; background: #EEEEEE; }
    )");
}
} // namespace Theme

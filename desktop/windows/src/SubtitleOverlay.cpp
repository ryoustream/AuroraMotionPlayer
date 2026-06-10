#include "SubtitleOverlay.h"
#include <QPainter>
#include <QPainterPath>

SubtitleOverlay::SubtitleOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(0, parent ? parent->height()-100 : 0, parent ? parent->width() : 800, 80);
}

void SubtitleOverlay::setText(const QString& t) {
    m_text = t; repaint();
}

void SubtitleOverlay::setStyle(const QString& font, int size, QColor color, QColor outline) {
    m_font=font; m_size=size; m_color=color; m_outline=outline; repaint();
}

void SubtitleOverlay::paintEvent(QPaintEvent*) {
    if (m_text.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f(m_font, m_size);
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    int tw = fm.horizontalAdvance(m_text);
    int x  = (width() - tw) / 2;
    int y  = height() - 16;
    QPainterPath path;
    path.addText(x, y, f, m_text);
    // Outline
    p.setPen(QPen(m_outline, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    // Fill
    p.fillPath(path, m_color);
}

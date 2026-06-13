#include "ThumbnailBar.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <cmath>

ThumbnailBar::ThumbnailBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kThumbH + 10);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
}

ThumbnailBar::~ThumbnailBar() = default;

void ThumbnailBar::setDuration(double durationSeconds)
{
    m_duration = durationSeconds;
    rebuildSlots();
}

void ThumbnailBar::setHoverPosition(double fraction)
{
    m_hoverFrac = fraction;
    update();
}

void ThumbnailBar::setThumbnail(int index, const QPixmap& thumb)
{
    if (index < 0 || index >= m_slots) return;
    m_thumbs[index] = thumb;
    update();
}

double ThumbnailBar::slotTime(int index) const
{
    if (m_slots == 0) return 0.0;
    double interval = m_duration / m_slots;
    return index * interval + interval * 0.5;
}

void ThumbnailBar::clear()
{
    m_duration  = 0.0;
    m_slots     = 0;
    m_hoverFrac = -1.0;
    m_thumbs.clear();
    update();
}

void ThumbnailBar::rebuildSlots()
{
    if (m_duration <= 0.0) { clear(); return; }

    int newSlots = qMin(static_cast<int>(kMaxSlots),
                        qMax(1, static_cast<int>(width() / 120.0)));
    m_slots = newSlots;
    m_thumbs.assign(m_slots, QPixmap());

    // Request thumbnails
    for (int i = 0; i < m_slots; ++i)
        emit thumbnailRequested(i, slotTime(i));

    update();
}

void ThumbnailBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_duration > 0.0) rebuildSlots();
}

void ThumbnailBar::paintEvent(QPaintEvent*)
{
    if (m_slots == 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    int W = width(), H = kThumbH;
    int slotW = W / m_slots;

    for (int i = 0; i < m_slots; ++i) {
        QRect r(i * slotW, 0, slotW, H);

        // Background
        p.fillRect(r, QColor(10, 10, 20, 200));

        // Thumbnail
        if (!m_thumbs[i].isNull()) {
            p.drawPixmap(r, m_thumbs[i].scaled(r.size(),
                         Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation));
        } else {
            // Placeholder gradient
            QLinearGradient grad(r.topLeft(), r.bottomRight());
            grad.setColorAt(0, QColor(20, 20, 40));
            grad.setColorAt(1, QColor(10, 10, 20));
            p.fillRect(r, grad);
            p.setPen(QColor(60, 60, 90));
            p.drawText(r, Qt::AlignCenter, "…");
        }

        // Thin separator
        p.setPen(QColor(0, 0, 0, 180));
        p.drawLine(r.topRight(), r.bottomRight());
    }

    // Hover highlight: the slot under cursor
    if (m_hoverFrac >= 0.0 && m_hoverFrac <= 1.0) {
        int hoverSlot = qMin(static_cast<int>(m_hoverFrac * m_slots), m_slots - 1);
        QRect hr(hoverSlot * slotW, 0, slotW, H);

        // White border around hovered slot
        p.setPen(QPen(QColor(180, 180, 255, 200), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(hr.adjusted(1, 1, -1, -1));

        // Time label
        double t = m_hoverFrac * m_duration;
        int    s = static_cast<int>(t);
        int    mm = s / 60, ss = s % 60;
        QString timeStr = QString("%1:%2").arg(mm).arg(ss, 2, 10, QChar('0'));

        QFont f = p.font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 230));
        p.drawText(hr, Qt::AlignBottom | Qt::AlignHCenter, timeStr);
    }

    // Bottom fade
    QLinearGradient fade(0, H - 12, 0, H);
    fade.setColorAt(0, Qt::transparent);
    fade.setColorAt(1, QColor(0, 0, 0, 160));
    p.fillRect(0, H - 12, W, 12, fade);
}

void ThumbnailBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_duration > 0.0) {
        double frac = static_cast<double>(e->pos().x()) / width();
        frac = qBound(0.0, frac, 1.0);
        emit seekRequested(frac * m_duration);
    }
}

void ThumbnailBar::mouseMoveEvent(QMouseEvent* e)
{
    if (m_duration > 0.0) {
        double frac = static_cast<double>(e->pos().x()) / width();
        m_hoverFrac = qBound(0.0, frac, 1.0);
        update();
    }
}

void ThumbnailBar::leaveEvent(QEvent*)
{
    m_hoverFrac = -1.0;
    update();
}

#include "BenchmarkOverlay.h"
#include <QPainter>
#include <QFontMetrics>

BenchmarkOverlay::BenchmarkOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedSize(300, 160);
    move(8, 8);
    hide();
}

void BenchmarkOverlay::update(double rFPS, double dFPS, double iFPS,
                               uint64_t dropped, double cpu, double gpu,
                               size_t vUsed, size_t vTotal) {
    m_renderFPS=rFPS; m_decodeFPS=dFPS; m_interpFPS=iFPS;
    m_dropped=dropped; m_cpu=cpu; m_gpu=gpu;
    m_vramUsed=vUsed; m_vramTotal=vTotal;
    repaint();
}

void BenchmarkOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 160));
    p.setPen(QColor(0, 200, 100));
    p.setFont(QFont("Consolas", 10));
    int y = 16;
    auto line = [&](const QString& txt) { p.drawText(8, y, txt); y += 16; };
    line(QString("Render FPS : %1").arg(m_renderFPS,   0,'f',1));
    line(QString("Decode FPS : %1").arg(m_decodeFPS,   0,'f',1));
    line(QString("Interp FPS : %1").arg(m_interpFPS,   0,'f',1));
    line(QString("Dropped    : %1").arg(m_dropped));
    p.setPen(QColor(200, 200, 80));
    line(QString("CPU        : %1%").arg(m_cpu,         0,'f',1));
    line(QString("GPU        : %1%").arg(m_gpu,         0,'f',1));
    line(QString("VRAM       : %1/%2 MB").arg(m_vramUsed).arg(m_vramTotal));
}

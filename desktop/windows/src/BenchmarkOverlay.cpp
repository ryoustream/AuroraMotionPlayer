// ============================================================================
//  Aurora Motion Player — BenchmarkOverlay.cpp (Session 10)
// ============================================================================
#include "BenchmarkOverlay.h"
#include "../../../core/benchmark/BenchmarkSystem.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QResizeEvent>
#include <algorithm>
#include <cmath>

using Snap = aurora::benchmark::BenchmarkSnapshot;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int kWidth       = 340;
static constexpr int kRowH        = 18;
static constexpr int kGraphH      = 44;
static constexpr int kPad         = 8;
static constexpr int kBarW        = 80;
static constexpr int kBarH        = 7;
static constexpr int kLabelW      = 90;

// ── Constructor ───────────────────────────────────────────────────────────────
BenchmarkOverlay::BenchmarkOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(kWidth);
    move(8, 8);
    hide();
    updateGeometry();
}

void BenchmarkOverlay::setVisible(bool v) {
    QWidget::setVisible(v);
}

void BenchmarkOverlay::resizeEvent(QResizeEvent*) {}

// ── updateSnapshot ────────────────────────────────────────────────────────────
void BenchmarkOverlay::updateSnapshot(const Snap& s) {
    m_renderFPS  = s.renderFPS;
    m_decodeFPS  = s.decodeFPS;
    m_interpFPS  = s.interpolateFPS;
    m_dropped    = s.droppedFrames;
    m_cpu        = s.cpuUsage;
    m_gpu        = s.gpuUsage;
    m_vramUsed   = s.vramUsedMB;
    m_vramTotal  = s.vramTotalMB;
    m_gpuTemp    = s.gpuTemperatureC;
    m_gpuPower   = s.gpuPowerWatts;
    m_gpuClock   = s.gpuClockMHz;
    m_cpuTemp    = s.cpuTemperatureC;
    m_avgFrameMs = s.avgFrameMs;
    m_frameVar   = s.frameVarianceMs;
    m_gpuName    = QString::fromStdString(s.gpuName);
    m_gpuVendor  = QString::fromStdString(s.gpuVendor);

    // Push to rolling graph
    auto push = [](std::deque<double>& d, double v) {
        d.push_back(v);
        if ((int)d.size() > BenchmarkOverlay::kGraphPoints) d.pop_front();
    };
    push(m_fpsHistory, m_renderFPS);
    push(m_gpuHistory, m_gpu);
    push(m_cpuHistory, m_cpu);

    // Recalculate height
    // sections: FPS | CPU | GPU | VRAM | Frame | GPU graph | CPU graph | footer
    int totalH = kPad
        + kRowH * 3  // FPS rows
        + 4
        + kRowH * 3  // CPU/GPU/VRAM rows
        + 4
        + kRowH      // Frame timing
        + 4
        + kGraphH + 4  // FPS graph
        + kGraphH + 4  // GPU/CPU graph
        + kRowH        // footer (GPU name)
        + kPad;
    setFixedSize(kWidth, totalH);
    repaint();
}

// ── Legacy update ─────────────────────────────────────────────────────────────
void BenchmarkOverlay::update(double rFPS, double dFPS, double iFPS,
                               uint64_t dropped, double cpu, double gpu,
                               size_t vUsed, size_t vTotal) {
    Snap s;
    s.renderFPS      = rFPS;
    s.decodeFPS      = dFPS;
    s.interpolateFPS = iFPS;
    s.droppedFrames  = dropped;
    s.cpuUsage       = cpu;
    s.gpuUsage       = gpu;
    s.vramUsedMB     = vUsed;
    s.vramTotalMB    = vTotal;
    updateSnapshot(s);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
QColor BenchmarkOverlay::lerp(QColor a, QColor b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(
        int(a.red()   + t * (b.red()   - a.red())),
        int(a.green() + t * (b.green() - a.green())),
        int(a.blue()  + t * (b.blue()  - a.blue())));
}

QColor BenchmarkOverlay::usageColor(double pct) {
    if (pct < 50)  return lerp({0, 200, 80}, {220, 220, 0}, pct / 50.0);
    if (pct < 80)  return lerp({220, 220, 0}, {255, 120, 0}, (pct-50)/30.0);
    return lerp({255, 120, 0}, {255, 40, 40}, (pct-80)/20.0);
}

void BenchmarkOverlay::drawBackground(QPainter& p, const QRect& r) {
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(r, 6, 6);
    p.fillPath(path, QColor(10, 12, 18, 210));
    p.setPen(QColor(60, 65, 80, 200));
    p.drawPath(path);
}

void BenchmarkOverlay::drawDivider(QPainter& p, int y) {
    p.setPen(QColor(50, 55, 70, 180));
    p.drawLine(kPad, y, width() - kPad, y);
}

void BenchmarkOverlay::drawSection(QPainter& p, int& y,
                                    const QString& label, const QString& val,
                                    double pct, QColor barColor) {
    // Label
    p.setPen(QColor(160, 170, 190));
    p.setFont(QFont("Consolas", 9));
    p.drawText(kPad, y, kLabelW, kRowH, Qt::AlignVCenter | Qt::AlignLeft, label);

    // Value
    p.setPen(QColor(230, 235, 245));
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    p.drawText(kPad + kLabelW, y, 70, kRowH, Qt::AlignVCenter | Qt::AlignLeft, val);

    // Bar
    if (pct >= 0) {
        int bx = width() - kPad - kBarW;
        int by = y + (kRowH - kBarH) / 2;
        // Background
        p.fillRect(bx, by, kBarW, kBarH, QColor(40, 45, 55));
        // Fill
        int fillW = int(kBarW * std::clamp(pct, 0.0, 100.0) / 100.0);
        if (fillW > 0) {
            QColor fill = barColor.isValid() ? barColor : usageColor(pct);
            p.fillRect(bx, by, fillW, kBarH, fill);
        }
        // Border
        p.setPen(QColor(80, 90, 110, 100));
        p.drawRect(bx, by, kBarW, kBarH);
    }
    y += kRowH;
}

void BenchmarkOverlay::drawMiniGraph(QPainter& p, const QRect& r,
                                      const std::deque<double>& data,
                                      double maxVal, QColor lineColor, QColor fillColor) {
    if (data.size() < 2) return;
    p.save();
    p.setClipRect(r);

    double xStep = double(r.width()) / (kGraphPoints - 1);
    auto yPos = [&](double v) -> double {
        return r.bottom() - (maxVal > 0 ? v / maxVal : 0.0) * r.height();
    };

    // Fill area
    QPainterPath fill;
    fill.moveTo(r.left(), r.bottom());
    int start = std::max(0, int(data.size()) - kGraphPoints);
    for (int i = start; i < (int)data.size(); ++i) {
        double x = r.left() + (i - start) * xStep;
        double y = yPos(data[i]);
        if (i == start) fill.lineTo(x, y);
        else            fill.lineTo(x, y);
    }
    fill.lineTo(r.right(), r.bottom());
    fill.closeSubpath();
    QColor fc = fillColor;
    fc.setAlpha(60);
    p.fillPath(fill, fc);

    // Line
    QPainterPath line;
    bool first = true;
    for (int i = start; i < (int)data.size(); ++i) {
        double x = r.left() + (i - start) * xStep;
        double y = yPos(data[i]);
        if (first) { line.moveTo(x, y); first = false; }
        else        line.lineTo(x, y);
    }
    p.setPen(QPen(lineColor, 1.5));
    p.drawPath(line);

    // Grid lines at 25%, 50%, 75%
    p.setPen(QColor(60, 65, 80, 100));
    for (int pct : {25, 50, 75}) {
        int gy = int(yPos(maxVal * pct / 100.0));
        p.drawLine(r.left(), gy, r.right(), gy);
    }

    p.restore();
}

// ── paintEvent ───────────────────────────────────────────────────────────────
void BenchmarkOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawBackground(p, rect());

    int y = kPad;

    // ── Header ─────────────────────────────────────────────────────────────
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    p.setPen(QColor(80, 200, 255));
    p.drawText(kPad, y, width(), kRowH,
               Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("Aurora Motion Player — Benchmark"));
    y += kRowH + 2;
    drawDivider(p, y); y += 4;

    // ── FPS section ────────────────────────────────────────────────────────
    // Treat 120 FPS as 100% for bar
    drawSection(p, y, "Render FPS", QString::number(m_renderFPS, 'f', 1),
                m_renderFPS / 120.0 * 100.0, QColor(0, 210, 130));
    drawSection(p, y, "Decode FPS", QString::number(m_decodeFPS, 'f', 1),
                m_decodeFPS / 120.0 * 100.0, QColor(60, 180, 255));
    drawSection(p, y, "Interp FPS", QString::number(m_interpFPS, 'f', 1),
                m_interpFPS / 120.0 * 100.0, QColor(180, 100, 255));
    drawSection(p, y, "Dropped",    QString::number(m_dropped),
                -1, {});

    drawDivider(p, y); y += 4;

    // ── CPU / GPU / VRAM ───────────────────────────────────────────────────
    drawSection(p, y, "CPU",
                QString("%1%  %2°C").arg(m_cpu, 0,'f',1).arg(m_cpuTemp, 0,'f',0),
                m_cpu, usageColor(m_cpu));
    drawSection(p, y, "GPU",
                QString("%1%  %2°C").arg(m_gpu, 0,'f',1).arg(m_gpuTemp, 0,'f',0),
                m_gpu, usageColor(m_gpu));
    {
        double vramPct = m_vramTotal > 0
            ? 100.0 * double(m_vramUsed) / double(m_vramTotal) : 0;
        drawSection(p, y, "VRAM",
                    QString("%1 / %2 MB").arg(m_vramUsed).arg(m_vramTotal),
                    vramPct, usageColor(vramPct));
    }

    drawDivider(p, y); y += 4;

    // ── Frame timing ───────────────────────────────────────────────────────
    drawSection(p, y, "Frame Time",
                QString("%1 ms ±%2").arg(m_avgFrameMs,0,'f',2).arg(m_frameVar,0,'f',2),
                std::clamp(m_avgFrameMs / 33.0 * 100.0, 0.0, 100.0),
                QColor(255, 180, 0));

    if (m_gpuPower > 0 || m_gpuClock > 0) {
        drawSection(p, y, "Power/Clk",
                    QString("%1W  %2MHz").arg(m_gpuPower,0,'f',1).arg(m_gpuClock,0,'f',0),
                    -1, {});
    }

    drawDivider(p, y); y += 4;

    // ── FPS graph ─────────────────────────────────────────────────────────
    {
        p.setPen(QColor(120, 130, 150));
        p.setFont(QFont("Consolas", 8));
        p.drawText(kPad, y, 80, 12, Qt::AlignVCenter, "FPS (0-120)");
        y += 12;
        QRect gr(kPad, y, width() - 2*kPad, kGraphH);
        p.fillRect(gr, QColor(20, 24, 32));
        drawMiniGraph(p, gr, m_fpsHistory, 120, QColor(0, 210, 130), QColor(0, 210, 130));
        p.setPen(QColor(60, 65, 80));
        p.drawRect(gr);
        y += kGraphH + 4;
    }

    // ── GPU/CPU graph ─────────────────────────────────────────────────────
    {
        p.setPen(QColor(120, 130, 150));
        p.setFont(QFont("Consolas", 8));
        p.drawText(kPad, y, 80, 12, Qt::AlignVCenter, "GPU/CPU %");
        y += 12;
        QRect gr(kPad, y, width() - 2*kPad, kGraphH);
        p.fillRect(gr, QColor(20, 24, 32));
        drawMiniGraph(p, gr, m_gpuHistory, 100, QColor(255, 120, 40), QColor(255, 80, 20));
        drawMiniGraph(p, gr, m_cpuHistory, 100, QColor(80, 160, 255), QColor(40, 100, 220));
        p.setPen(QColor(60, 65, 80));
        p.drawRect(gr);
        y += kGraphH + 4;
    }

    drawDivider(p, y); y += 4;

    // ── GPU footer ─────────────────────────────────────────────────────────
    p.setFont(QFont("Consolas", 8));
    p.setPen(QColor(100, 110, 130));
    QString footer = m_gpuName.isEmpty()
        ? QStringLiteral("GPU: Unknown")
        : QString("[%1] %2").arg(m_gpuVendor, m_gpuName);
    p.drawText(kPad, y, width()-kPad*2, kRowH, Qt::AlignVCenter, footer);
}

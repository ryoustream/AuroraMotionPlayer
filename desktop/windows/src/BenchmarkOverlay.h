#pragma once
// ============================================================================
//  Aurora Motion Player — BenchmarkOverlay (Session 10)
//  Full-featured stats HUD: live graph, color-coded metrics, GPU info
// ============================================================================

#include <QWidget>
#include <deque>
#include <cstddef>
#include <cstdint>
#include <QString>

class QTimer;

namespace aurora::benchmark { struct BenchmarkSnapshot; }

class BenchmarkOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BenchmarkOverlay(QWidget* parent = nullptr);

    /// Update with a full snapshot (called from BenchmarkSystem callback).
    void updateSnapshot(const aurora::benchmark::BenchmarkSnapshot& snap);

    /// Legacy update method (compatible with Session 6 usage)
    void update(double renderFPS, double decodeFPS, double interpFPS,
                uint64_t dropped, double cpuPct, double gpuPct,
                size_t vramUsed, size_t vramTotal);

    void setVisible(bool v) override;
    void toggleVisible() { setVisible(!isVisible()); }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    // ── Current stats ─────────────────────────────────────────────────────────
    double   m_renderFPS    = 0;
    double   m_decodeFPS    = 0;
    double   m_interpFPS    = 0;
    uint64_t m_dropped      = 0;
    double   m_cpu          = 0;
    double   m_gpu          = 0;
    size_t   m_vramUsed     = 0;
    size_t   m_vramTotal    = 0;
    double   m_gpuTemp      = 0;
    double   m_gpuPower     = 0;
    double   m_gpuClock     = 0;
    double   m_cpuTemp      = 0;
    double   m_avgFrameMs   = 0;
    double   m_frameVar     = 0;
    QString  m_gpuName;
    QString  m_gpuVendor;

    // ── Rolling graph data (last N frames) ───────────────────────────────────
    static constexpr int kGraphPoints = 60;
    std::deque<double> m_fpsHistory;
    std::deque<double> m_gpuHistory;
    std::deque<double> m_cpuHistory;

    // ── Paint helpers ─────────────────────────────────────────────────────────
    void drawBackground(QPainter& p, const QRect& r);
    void drawSection(QPainter& p, int& y, const QString& label, const QString& val,
                     double pct, QColor barColor);
    void drawMiniGraph(QPainter& p, const QRect& r, const std::deque<double>& data,
                       double maxVal, QColor lineColor, QColor fillColor);
    void drawDivider(QPainter& p, int y);

    static QColor lerp(QColor a, QColor b, double t);
    static QColor usageColor(double pct);
};

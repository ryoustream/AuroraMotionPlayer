#pragma once
#include <QWidget>
class QLabel;
class QTimer;
class BenchmarkOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BenchmarkOverlay(QWidget* parent = nullptr);
    void update(double renderFPS, double decodeFPS, double interpFPS,
                uint64_t dropped, double cpuPct, double gpuPct,
                size_t vramUsed, size_t vramTotal);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    double m_renderFPS=0, m_decodeFPS=0, m_interpFPS=0;
    double m_cpu=0, m_gpu=0;
    uint64_t m_dropped=0;
    size_t m_vramUsed=0, m_vramTotal=0;
};

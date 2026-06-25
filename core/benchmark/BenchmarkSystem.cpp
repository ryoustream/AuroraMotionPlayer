// ============================================================================
//  Aurora Motion Player — BenchmarkSystem.cpp (Session 10)
// ============================================================================
#include "BenchmarkSystem.h"
#include "../gpu_monitor/GPUMonitor.h"
#include "../gpu_monitor/CPUMonitor.h"

#include <sstream>
#include <iomanip>
#include <numeric>
#include <cmath>

namespace aurora::benchmark {

BenchmarkSystem::BenchmarkSystem()
    : m_gpu(std::make_unique<GPUMonitor>())
    , m_cpu(std::make_unique<CPUMonitor>())
{}

BenchmarkSystem::~BenchmarkSystem() {
    stop();
}

void BenchmarkSystem::start() {
    m_running              = true;
    m_startTime            = Clock::now();
    m_decodedFrames        = 0;
    m_renderedFrames       = 0;
    m_interpolatedFrames   = 0;
    m_droppedFrames        = 0;

    m_gpu->init();
    m_cpu->init();
}

void BenchmarkSystem::stop() {
    m_running.store(false);
    if (m_cbThread.joinable()) m_cbThread.join();
    m_gpu->shutdown();
    m_cpu->shutdown();
}

void BenchmarkSystem::onFrameDecoded()      { ++m_decodedFrames;      }
void BenchmarkSystem::onFrameRendered()     { ++m_renderedFrames;      }
void BenchmarkSystem::onFrameInterpolated() { ++m_interpolatedFrames;  }
void BenchmarkSystem::onFrameDropped()      { ++m_droppedFrames;       }

void BenchmarkSystem::onRenderBegin() {
    m_renderBegin = Clock::now();
}

void BenchmarkSystem::onRenderEnd() {
    auto now = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - m_renderBegin).count();
    std::lock_guard lock(m_mutex);
    m_renderTimes.push_back(now);
    m_frameDurMs.push_back(ms);
    // Rolling window of 1 second
    while (m_renderTimes.size() > 1 &&
           std::chrono::duration<double>(now - m_renderTimes.front()).count() > 1.0) {
        m_renderTimes.pop_front();
        m_frameDurMs.pop_front();
    }
}

BenchmarkSnapshot BenchmarkSystem::snapshot() const {
    BenchmarkSnapshot s;
    std::lock_guard lock(m_mutex);

    double elapsed = std::chrono::duration<double>(Clock::now() - m_startTime).count();
    if (elapsed > 0.0) {
        s.decodeFPS      = double(m_decodedFrames)      / elapsed;
        s.renderFPS      = double(m_renderedFrames)      / elapsed;
        s.interpolateFPS = double(m_interpolatedFrames)  / elapsed;
    }
    // Rolling window FPS
    if (m_renderTimes.size() >= 2) {
        double window = std::chrono::duration<double>(
            m_renderTimes.back() - m_renderTimes.front()).count();
        if (window > 0.0)
            s.renderFPS = double(m_renderTimes.size() - 1) / window;
    }

    s.droppedFrames = m_droppedFrames.load();

    if (!m_frameDurMs.empty()) {
        double sum = std::accumulate(m_frameDurMs.begin(), m_frameDurMs.end(), 0.0);
        s.avgFrameMs = sum / double(m_frameDurMs.size());
        double var = 0.0;
        for (double d : m_frameDurMs) var += (d - s.avgFrameMs) * (d - s.avgFrameMs);
        s.frameVarianceMs = std::sqrt(var / double(m_frameDurMs.size()));
    }

    // GPU stats from GPUMonitor
    auto gpuSample = m_gpu->latestSample();
    auto gpuInfo   = m_gpu->info();
    s.gpuUsage        = gpuSample.gpuUsagePct;
    s.vramUsedMB      = gpuSample.vramUsedMB;
    s.vramTotalMB     = gpuSample.vramTotalMB;
    s.gpuTemperatureC = gpuSample.temperatureC;
    s.gpuPowerWatts   = gpuSample.powerWatts;
    s.gpuClockMHz     = gpuSample.clockMHz;
    s.gpuName         = gpuInfo.name;
    s.gpuVendor       = gpuInfo.vendor;

    // CPU stats from CPUMonitor
    auto cpuSample    = m_cpu->latestSample();
    s.cpuUsage        = cpuSample.totalUsagePct;
    s.cpuTemperatureC = cpuSample.temperatureC;

    return s;
}

void BenchmarkSystem::setCallback(SnapshotCallback cb, int intervalMs) {
    m_callback   = std::move(cb);
    m_intervalMs = intervalMs;

    // Start background callback thread
    if (m_cbThread.joinable()) {
        m_running.store(false);
        m_cbThread.join();
        m_running.store(true);
    }
    // Also start polling on monitors
    m_gpu->startPolling(intervalMs);
    m_cpu->startPolling(intervalMs);

    m_cbThread = std::thread(&BenchmarkSystem::callbackLoop, this);
}

void BenchmarkSystem::callbackLoop() {
    while (m_running.load()) {
        if (m_callback) m_callback(snapshot());
        std::this_thread::sleep_for(std::chrono::milliseconds(m_intervalMs));
    }
}

std::string BenchmarkSystem::format(const BenchmarkSnapshot& s) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Render: "  << s.renderFPS        << " fps | ";
    oss << "Decode: "  << s.decodeFPS        << " fps | ";
    oss << "Interp: "  << s.interpolateFPS   << " fps | ";
    oss << "Dropped: " << s.droppedFrames    << " | ";
    oss << "CPU: "     << s.cpuUsage         << "% | ";
    oss << "GPU: "     << s.gpuUsage         << "% [" << s.gpuName << "] | ";
    oss << "VRAM: "    << s.vramUsedMB       << "/" << s.vramTotalMB << " MB | ";
    oss << "Temp: "    << s.gpuTemperatureC  << "°C | ";
    oss << "Power: "   << s.gpuPowerWatts    << " W | ";
    oss << "Frame: "   << s.avgFrameMs       << " ms ±" << s.frameVarianceMs << " ms";
    return oss.str();
}

} // namespace aurora::benchmark

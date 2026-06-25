#pragma once
// ============================================================================
//  Aurora Motion Player — BenchmarkSystem (Session 10 upgrade)
//  Integrates GPUMonitor + CPUMonitor for real-time stats
// ============================================================================

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <functional>
#include <thread>
#include <memory>

namespace aurora::benchmark {

class GPUMonitor;
class CPUMonitor;

struct BenchmarkSnapshot {
    double   cpuUsage         = 0.0;  // %
    double   gpuUsage         = 0.0;  // %
    size_t   vramUsedMB       = 0;
    size_t   vramTotalMB      = 0;
    double   renderFPS        = 0.0;
    double   decodeFPS        = 0.0;
    double   interpolateFPS   = 0.0;
    uint64_t droppedFrames    = 0;
    double   avgFrameMs       = 0.0;
    double   frameVarianceMs  = 0.0;
    double   gpuTemperatureC  = 0.0;
    double   gpuPowerWatts    = 0.0;
    double   gpuClockMHz      = 0.0;
    double   cpuTemperatureC  = 0.0;
    std::string gpuName;
    std::string gpuVendor;
};

class BenchmarkSystem {
public:
    using SnapshotCallback = std::function<void(const BenchmarkSnapshot&)>;

    BenchmarkSystem();
    ~BenchmarkSystem();

    // Non-copyable
    BenchmarkSystem(const BenchmarkSystem&)            = delete;
    BenchmarkSystem& operator=(const BenchmarkSystem&) = delete;

    void start();
    void stop();

    // Frame event hooks — call from player pipeline
    void onFrameDecoded();
    void onFrameRendered();
    void onFrameInterpolated();
    void onFrameDropped();
    void onRenderBegin();
    void onRenderEnd();

    BenchmarkSnapshot snapshot() const;

    /// Register a callback fired every intervalMs by background thread.
    void setCallback(SnapshotCallback cb, int intervalMs = 500);

    static std::string format(const BenchmarkSnapshot& snap);

    // Access underlying monitors (for direct queries)
    GPUMonitor* gpuMonitor() const { return m_gpu.get(); }
    CPUMonitor* cpuMonitor() const { return m_cpu.get(); }

private:
    void callbackLoop();

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    std::unique_ptr<GPUMonitor> m_gpu;
    std::unique_ptr<CPUMonitor> m_cpu;

    std::atomic<bool>     m_running{false};
    std::atomic<uint64_t> m_decodedFrames{0};
    std::atomic<uint64_t> m_renderedFrames{0};
    std::atomic<uint64_t> m_interpolatedFrames{0};
    std::atomic<uint64_t> m_droppedFrames{0};

    mutable std::mutex        m_mutex;
    std::deque<TimePoint>     m_renderTimes;
    std::deque<double>        m_frameDurMs;
    TimePoint                 m_renderBegin;
    TimePoint                 m_startTime;

    SnapshotCallback          m_callback;
    int                       m_intervalMs = 500;
    std::thread               m_cbThread;
};

} // namespace aurora::benchmark

#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <functional>

namespace aurora::benchmark {

struct BenchmarkSnapshot {
    double cpuUsage         = 0.0;  // %
    double gpuUsage         = 0.0;  // %
    size_t vramUsedMB       = 0;
    size_t vramTotalMB      = 0;
    double renderFPS        = 0.0;
    double decodeFPS        = 0.0;
    double interpolateFPS   = 0.0;
    uint64_t droppedFrames  = 0;
    double avgFrameMs       = 0.0;
    double frameVarianceMs  = 0.0;
};

class BenchmarkSystem {
public:
    using SnapshotCallback = std::function<void(const BenchmarkSnapshot&)>;

    BenchmarkSystem();
    ~BenchmarkSystem() = default;

    void start();
    void stop();

    void onFrameDecoded();
    void onFrameRendered();
    void onFrameInterpolated();
    void onFrameDropped();
    void onRenderBegin();
    void onRenderEnd();

    BenchmarkSnapshot snapshot() const;
    void setCallback(SnapshotCallback cb, int intervalMs = 500);

    // Format snapshot as human-readable string
    static std::string format(const BenchmarkSnapshot& snap);

private:
    void update();

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    std::atomic<bool>     m_running{false};
    std::atomic<uint64_t> m_decodedFrames{0};
    std::atomic<uint64_t> m_renderedFrames{0};
    std::atomic<uint64_t> m_interpolatedFrames{0};
    std::atomic<uint64_t> m_droppedFrames{0};

    mutable std::mutex          m_mutex;
    std::deque<TimePoint>       m_renderTimes;  // rolling window
    std::deque<double>          m_frameDurMs;
    TimePoint                   m_renderBegin;
    TimePoint                   m_startTime;

    SnapshotCallback            m_callback;
    int                         m_intervalMs = 500;

    // Platform-specific CPU/GPU usage
    double readCPUUsage()  const;
    double readGPUUsage()  const;
    size_t readVRAMUsed()  const;
    size_t readVRAMTotal() const;
};

} // namespace aurora::benchmark

#pragma once
// ============================================================================
//  Aurora Motion Player — CPUMonitor
//  Session 10: GPU Benchmark System
//
//  Cross-platform CPU stats:
//    - Windows: GetSystemTimes (aggregate) + NtQuerySystemInformation (per-core)
//    - Linux  : /proc/stat
//    - Android: /proc/stat + /sys/devices/system/cpu thermal
// ============================================================================

#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace aurora::benchmark {

struct CPUSample {
    double   totalUsagePct  = 0.0;         ///< Aggregate usage across all cores
    std::vector<double> coreUsagePct;      ///< Per-core usage (may be empty)
    double   temperatureC   = 0.0;         ///< Package temperature (if available)
    uint64_t totalUsTimeUs  = 0;           ///< Total user+sys time (microseconds)
};

class CPUMonitor {
public:
    CPUMonitor();
    ~CPUMonitor();

    CPUMonitor(const CPUMonitor&)            = delete;
    CPUMonitor& operator=(const CPUMonitor&) = delete;

    bool init();
    void shutdown();

    CPUSample sample();   ///< Non-const: computes delta from last call

    using SampleCallback = std::function<void(const CPUSample&)>;
    void startPolling(int intervalMs = 500, SampleCallback cb = nullptr);
    void stopPolling();

    CPUSample latestSample() const;

    static std::string formatSample(const CPUSample& s);

private:
    void pollLoop(int intervalMs, SampleCallback cb);

    // Per-platform internals
    struct CoreState {
        uint64_t user   = 0;
        uint64_t nice   = 0;
        uint64_t sys    = 0;
        uint64_t idle   = 0;
        uint64_t total  = 0;
    };

    bool                        m_initialised = false;
    std::vector<CoreState>      m_prevState;
    std::atomic<bool>           m_polling{false};
    std::thread                 m_pollThread;
    mutable std::mutex          m_mutex;
    CPUSample                   m_latestSample;

    // Platform
    CPUSample samplePlatform();
    CPUSample sampleProc();       // Linux / Android /proc/stat

#ifdef _WIN32
    CPUSample sampleWindows();
    struct WinCoreState {
        uint64_t idle   = 0;
        uint64_t kernel = 0;
        uint64_t user   = 0;
    };
    std::vector<WinCoreState> m_winPrev;
#endif
};

} // namespace aurora::benchmark

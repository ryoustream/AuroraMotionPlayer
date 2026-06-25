#pragma once
// ============================================================================
//  Aurora Motion Player — GPUMonitor
//  Session 10: GPU Benchmark System
//
//  Abstracts GPU usage / VRAM queries across:
//    - Windows: D3DKMT (all vendors) + NVML (NVIDIA extended stats)
//    - Linux  : /sys/class/drm + NVML
//    - Android: /sys/kernel/gpu (Adreno/kgsl), /sys/bus/platform (Mali)
//
//  All methods are thread-safe. Queries are cheap (< 1 ms) on all paths.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <thread>
#include <chrono>

namespace aurora::benchmark {

// ---------------------------------------------------------------------------
//  GPU information
// ---------------------------------------------------------------------------
struct GPUInfo {
    std::string name;
    std::string vendor;      // "NVIDIA" | "AMD" | "Intel" | "Qualcomm" | "ARM" | "Unknown"
    uint32_t    deviceId  = 0;
    uint32_t    vendorId  = 0;
    size_t      vramTotalMB = 0;
    bool        nvmlAvailable = false;  // NVML path active
    bool        d3dkmtAvailable = false; // D3DKMT path active
};

// ---------------------------------------------------------------------------
//  Per-sample GPU statistics
// ---------------------------------------------------------------------------
struct GPUSample {
    double   gpuUsagePct  = 0.0;   // GPU engine (3D/compute) utilisation %
    double   memUsagePct  = 0.0;   // VRAM utilisation %
    size_t   vramUsedMB   = 0;
    size_t   vramTotalMB  = 0;
    double   temperatureC = 0.0;   // degrees Celsius (NVML / sysfs only)
    double   powerWatts   = 0.0;   // power draw (NVML only)
    double   clockMHz     = 0.0;   // GPU core clock (NVML / sysfs)
    uint32_t fanRPM       = 0;     // fan speed (NVML only)
};

// ---------------------------------------------------------------------------
//  GPUMonitor
// ---------------------------------------------------------------------------
class GPUMonitor {
public:
    GPUMonitor();
    ~GPUMonitor();

    // Non-copyable
    GPUMonitor(const GPUMonitor&)            = delete;
    GPUMonitor& operator=(const GPUMonitor&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────────────────
    /// Detect GPU and initialise the best available backend.
    /// Returns true if at least one query path is available.
    bool init();

    /// Release backend resources (NVML shutdown, handles closed).
    void shutdown();

    // ── Query ────────────────────────────────────────────────────────────────
    GPUSample   sample()  const;        ///< Blocking single sample (< 1 ms)
    GPUInfo     info()    const;        ///< Static info (filled on init)

    // ── Polling thread ───────────────────────────────────────────────────────
    using SampleCallback = std::function<void(const GPUSample&)>;

    /// Start background polling at given interval.
    void startPolling(int intervalMs = 500, SampleCallback cb = nullptr);
    void stopPolling();
    bool isPolling() const { return m_polling.load(); }

    /// Latest cached sample (updated by polling thread).
    GPUSample latestSample() const;

    // ── Static helpers ───────────────────────────────────────────────────────
    static std::string formatSample(const GPUSample& s);

private:
    // Backend implementations
    bool initNVML();
    bool initD3DKMT();
    bool initSysfs();       // Linux / Android sysfs
    bool initAndroidKgsl(); // Adreno kgsl sysfs
    bool initAndroidMali(); // Mali sysfs

    GPUSample sampleNVML()       const;
    GPUSample sampleD3DKMT()     const;
    GPUSample sampleSysfs()      const;
    GPUSample sampleAndroidKgsl() const;
    GPUSample sampleAndroidMali() const;

    void pollLoop(int intervalMs, SampleCallback cb);

    // ── State ─────────────────────────────────────────────────────────────────
    enum class Backend {
        None,
        NVML,
        D3DKMT,
        Sysfs,
        AndroidKgsl,
        AndroidMali,
    };

    Backend  m_backend = Backend::None;
    GPUInfo  m_info;

    // NVML opaque handle (void* to avoid dragging in nvml.h everywhere)
    void*    m_nvmlDevice = nullptr;
    bool     m_nvmlLoaded = false;

    // D3DKMT adapter LUID / handle (Windows)
    uint64_t m_d3dLuid    = 0;
    void*    m_d3dAdapter = nullptr; // HANDLE

    // Android sysfs paths
    std::string m_kgslBusyPath;
    std::string m_kgslFreqPath;
    std::string m_kgslMemPath;
    std::string m_maliLoadPath;
    std::string m_maliFreqPath;

    // Linux sysfs paths
    std::string m_drmCardPath;

    // Polling
    std::atomic<bool>       m_polling{false};
    std::thread             m_pollThread;
    mutable std::mutex      m_sampleMutex;
    GPUSample               m_latestSample;
};

} // namespace aurora::benchmark

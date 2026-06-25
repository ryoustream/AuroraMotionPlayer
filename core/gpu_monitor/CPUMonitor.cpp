// ============================================================================
//  Aurora Motion Player — CPUMonitor.cpp
//  Session 10: GPU Benchmark System
// ============================================================================

#include "CPUMonitor.h"
#include <sstream>
#include <iomanip>
#include <numeric>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#if defined(__linux__) || defined(__ANDROID__)
#  include <fstream>
#  include <string>
#endif

namespace aurora::benchmark {

// ── Helpers ──────────────────────────────────────────────────────────────────
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::string s((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());
    return s;
}

CPUMonitor::CPUMonitor()  = default;
CPUMonitor::~CPUMonitor() { shutdown(); }

bool CPUMonitor::init() {
    // Prime delta state
    sample();
    m_initialised = true;
    return true;
}

void CPUMonitor::shutdown() {
    stopPolling();
    m_initialised = false;
}

CPUSample CPUMonitor::sample() {
    return samplePlatform();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Windows
// ─────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32
CPUSample CPUMonitor::samplePlatform() { return sampleWindows(); }

CPUSample CPUMonitor::sampleWindows() {
    CPUSample s;

    // Aggregate via GetSystemTimes
    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) return s;

    auto ft2u64 = [](FILETIME ft) -> uint64_t {
        return (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };
    uint64_t idle   = ft2u64(ftIdle);
    uint64_t kernel = ft2u64(ftKernel);
    uint64_t user   = ft2u64(ftUser);

    if (m_winPrev.empty()) {
        m_winPrev.resize(1);
        m_winPrev[0] = { idle, kernel, user };
        return s;
    }
    auto& prev = m_winPrev[0];
    uint64_t dIdle   = idle   - prev.idle;
    uint64_t dKernel = kernel - prev.kernel;
    uint64_t dUser   = user   - prev.user;
    prev = { idle, kernel, user };

    uint64_t total = dKernel + dUser;
    if (total > 0)
        s.totalUsagePct = 100.0 * (1.0 - double(dIdle) / double(total));

    // Temperature via WMI is expensive; try MSAcpi_ThermalZoneTemperature
    // Omit for simplicity — returns 0.0
    return s;
}

CPUSample CPUMonitor::sampleProc() { return sampleWindows(); }

#else
// ─────────────────────────────────────────────────────────────────────────────
//  Linux / Android — /proc/stat
// ─────────────────────────────────────────────────────────────────────────────
CPUSample CPUMonitor::samplePlatform() { return sampleProc(); }

CPUSample CPUMonitor::sampleProc() {
    CPUSample s;
    std::ifstream stat("/proc/stat");
    if (!stat) return s;

    std::vector<CoreState> cur;
    std::string line;
    while (std::getline(stat, line)) {
        if (line.rfind("cpu", 0) != 0) break;
        CoreState cs{};
        unsigned long long u=0, n=0, sy=0, id=0, iow=0, irq=0, softirq=0, steal=0;
        const char* p = line.c_str();
        while (*p && !std::isspace((unsigned char)*p)) ++p; // skip "cpu" or "cpu0"
        sscanf(p, " %llu %llu %llu %llu %llu %llu %llu %llu",
               &u, &n, &sy, &id, &iow, &irq, &softirq, &steal);
        cs.user = u; cs.nice = n; cs.sys = sy; cs.idle = id;
        cs.total = u + n + sy + id + iow + irq + softirq + steal;
        cur.push_back(cs);
    }

    if (m_prevState.empty() || m_prevState.size() != cur.size()) {
        m_prevState = cur;
        return s;
    }

    // Compute deltas
    for (size_t i = 0; i < cur.size(); ++i) {
        uint64_t dTotal  = cur[i].total - m_prevState[i].total;
        uint64_t dIdle   = cur[i].idle  - m_prevState[i].idle;
        double   usage   = dTotal > 0
            ? 100.0 * (1.0 - double(dIdle) / double(dTotal))
            : 0.0;
        if (i == 0) s.totalUsagePct = usage; // first row = aggregate
        else        s.coreUsagePct.push_back(usage);
    }
    m_prevState = cur;

    // Temperature: /sys/class/thermal/thermal_zone0/temp
    {
        std::ifstream tf("/sys/class/thermal/thermal_zone0/temp");
        long long mC = 0;
        if (tf >> mC) s.temperatureC = mC / 1000.0;
    }
    return s;
}
#endif

// ── Polling ───────────────────────────────────────────────────────────────────
void CPUMonitor::startPolling(int intervalMs, SampleCallback cb) {
    if (m_polling.exchange(true)) return;
    m_pollThread = std::thread(&CPUMonitor::pollLoop, this, intervalMs, std::move(cb));
}

void CPUMonitor::stopPolling() {
    m_polling.store(false);
    if (m_pollThread.joinable()) m_pollThread.join();
}

CPUSample CPUMonitor::latestSample() const {
    std::lock_guard lock(m_mutex);
    return m_latestSample;
}

void CPUMonitor::pollLoop(int intervalMs, SampleCallback cb) {
    while (m_polling.load()) {
        auto s = sample();
        {
            std::lock_guard lock(m_mutex);
            m_latestSample = s;
        }
        if (cb) cb(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

std::string CPUMonitor::formatSample(const CPUSample& s) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "CPU: " << s.totalUsagePct << "%";
    if (s.temperatureC > 0) oss << " | Temp: " << s.temperatureC << "°C";
    return oss.str();
}

} // namespace aurora::benchmark

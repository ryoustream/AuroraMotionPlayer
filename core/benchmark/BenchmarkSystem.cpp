#include "BenchmarkSystem.h"
#include <sstream>
#include <iomanip>
#include <numeric>
#include <cmath>

#ifdef _WIN32
#  include <windows.h>
#  include <pdh.h>
#elif __linux__
#  include <fstream>
#endif

namespace aurora::benchmark {

BenchmarkSystem::BenchmarkSystem() = default;

void BenchmarkSystem::start() {
    m_running     = true;
    m_startTime   = Clock::now();
    m_decodedFrames      = 0;
    m_renderedFrames     = 0;
    m_interpolatedFrames = 0;
    m_droppedFrames      = 0;
}

void BenchmarkSystem::stop() {
    m_running = false;
}

void BenchmarkSystem::onFrameDecoded()      { ++m_decodedFrames;      }
void BenchmarkSystem::onFrameRendered()     { ++m_renderedFrames;     }
void BenchmarkSystem::onFrameInterpolated() { ++m_interpolatedFrames; }
void BenchmarkSystem::onFrameDropped()      { ++m_droppedFrames;      }

void BenchmarkSystem::onRenderBegin() {
    m_renderBegin = Clock::now();
}

void BenchmarkSystem::onRenderEnd() {
    auto now = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(now - m_renderBegin).count();
    std::lock_guard lock(m_mutex);
    m_renderTimes.push_back(now);
    m_frameDurMs.push_back(ms);
    // Keep rolling window of 1 second
    while (m_renderTimes.size() > 1 &&
           std::chrono::duration<double>(now - m_renderTimes.front()).count() > 1.0) {
        m_renderTimes.pop_front();
        m_frameDurMs.pop_front();
    }
}

BenchmarkSnapshot BenchmarkSystem::snapshot() const {
    BenchmarkSnapshot s;
    std::lock_guard lock(m_mutex);

    double elapsed = std::chrono::duration<double>(
        Clock::now() - m_startTime).count();

    if (elapsed > 0.0) {
        s.decodeFPS      = static_cast<double>(m_decodedFrames)      / elapsed;
        s.renderFPS      = static_cast<double>(m_renderedFrames)      / elapsed;
        s.interpolateFPS = static_cast<double>(m_interpolatedFrames)  / elapsed;
    }
    // Rolling FPS from render window
    if (m_renderTimes.size() >= 2) {
        double window = std::chrono::duration<double>(
            m_renderTimes.back() - m_renderTimes.front()).count();
        if (window > 0.0)
            s.renderFPS = static_cast<double>(m_renderTimes.size() - 1) / window;
    }

    s.droppedFrames = m_droppedFrames.load();

    if (!m_frameDurMs.empty()) {
        double sum = std::accumulate(m_frameDurMs.begin(), m_frameDurMs.end(), 0.0);
        s.avgFrameMs = sum / m_frameDurMs.size();
        double var = 0.0;
        for (double d : m_frameDurMs) var += (d - s.avgFrameMs) * (d - s.avgFrameMs);
        s.frameVarianceMs = std::sqrt(var / m_frameDurMs.size());
    }

    s.cpuUsage  = readCPUUsage();
    s.gpuUsage  = readGPUUsage();
    s.vramUsedMB  = readVRAMUsed();
    s.vramTotalMB = readVRAMTotal();

    return s;
}

void BenchmarkSystem::setCallback(SnapshotCallback cb, int intervalMs) {
    m_callback   = std::move(cb);
    m_intervalMs = intervalMs;
}

std::string BenchmarkSystem::format(const BenchmarkSnapshot& s) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Render: "   << s.renderFPS        << " fps | ";
    oss << "Decode: "   << s.decodeFPS        << " fps | ";
    oss << "Interp: "   << s.interpolateFPS   << " fps | ";
    oss << "Dropped: "  << s.droppedFrames    << " | ";
    oss << "CPU: "      << s.cpuUsage         << "% | ";
    oss << "GPU: "      << s.gpuUsage         << "% | ";
    oss << "VRAM: "     << s.vramUsedMB       << "/" << s.vramTotalMB << " MB | ";
    oss << "Frame: "    << s.avgFrameMs       << " ms ±" << s.frameVarianceMs << " ms";
    return oss.str();
}

// ── Platform CPU/GPU stats ────────────────────────────────────────────────────
double BenchmarkSystem::readCPUUsage() const {
#ifdef _WIN32
    // Simplified: use GetSystemTimes
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        static FILETIME prevIdle{}, prevKernel{}, prevUser{};
        auto ft2u64 = [](FILETIME ft) -> uint64_t {
            return (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        uint64_t dIdle   = ft2u64(idle)   - ft2u64(prevIdle);
        uint64_t dKernel = ft2u64(kernel) - ft2u64(prevKernel);
        uint64_t dUser   = ft2u64(user)   - ft2u64(prevUser);
        prevIdle = idle; prevKernel = kernel; prevUser = user;
        uint64_t total = dKernel + dUser;
        if (total > 0) return 100.0 * (1.0 - double(dIdle) / double(total));
    }
#elif defined(__linux__)
    std::ifstream stat("/proc/stat");
    std::string line;
    if (std::getline(stat, line)) {
        unsigned long long u, n, s, i;
        sscanf(line.c_str() + 5, "%llu %llu %llu %llu", &u, &n, &s, &i);
        double total = u + n + s + i;
        if (total > 0) return 100.0 * (1.0 - double(i) / total);
    }
#endif
    return 0.0;
}

double BenchmarkSystem::readGPUUsage() const {
    // NVML or D3DKMT query — stub
    return 0.0;
}

size_t BenchmarkSystem::readVRAMUsed() const  { return 0; }
size_t BenchmarkSystem::readVRAMTotal() const { return 0; }

} // namespace aurora::benchmark

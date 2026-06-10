#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>

namespace aurora::benchmark {

// Rolling FPS counter using a sliding time window
class FrameCounter {
public:
    explicit FrameCounter(double windowSeconds = 1.0)
        : m_window(windowSeconds) {}

    void tick() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(m_mutex);
        m_times.push_back(now);
        // Evict timestamps outside window
        while (!m_times.empty()) {
            double age = std::chrono::duration<double>(
                now - m_times.front()).count();
            if (age > m_window) m_times.pop_front();
            else break;
        }
    }

    double fps() const {
        std::lock_guard lock(m_mutex);
        if (m_times.size() < 2) return 0.0;
        double span = std::chrono::duration<double>(
            m_times.back() - m_times.front()).count();
        return (span > 0.0) ? (m_times.size() - 1) / span : 0.0;
    }

    void reset() {
        std::lock_guard lock(m_mutex);
        m_times.clear();
    }

private:
    using Clock = std::chrono::steady_clock;
    using TP    = Clock::time_point;

    double                    m_window;
    mutable std::mutex        m_mutex;
    std::deque<TP>            m_times;
};

} // namespace aurora::benchmark

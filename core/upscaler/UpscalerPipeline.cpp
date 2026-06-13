// ─────────────────────────────────────────────────────────────────────────────
// UpscalerPipeline.cpp  —  Aurora Motion Player
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerPipeline.h"
#include <chrono>
#include <cassert>

namespace aurora::upscaler {

UpscalerPipeline::UpscalerPipeline() = default;

UpscalerPipeline::~UpscalerPipeline() {
    shutdown();
}

bool UpscalerPipeline::init(const PipelineConfig& cfg) {
    shutdown(); // ensure clean state

    m_cfg = cfg;

    if (!cfg.passThrough) {
        m_upscaler = UpscalerFactory::create(cfg.upscaler.model);
        if (!m_upscaler->init(cfg.upscaler)) {
            // Fall back to pass-through on init failure
            m_cfg.passThrough = true;
        }
    }

    m_running = true;
    m_worker  = std::thread(&UpscalerPipeline::workerLoop, this);
    m_initialized = true;
    return true;
}

void UpscalerPipeline::shutdown() {
    if (!m_initialized) return;

    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        m_running = false;
        // Drain queue
        while (!m_queue.empty()) m_queue.pop();
    }
    m_queueCv.notify_all();

    if (m_worker.joinable()) m_worker.join();

    if (m_upscaler) {
        m_upscaler->shutdown();
        m_upscaler.reset();
    }

    m_initialized = false;
}

bool UpscalerPipeline::pushFrame(video::VideoFramePtr frame) {
    if (!frame || !m_initialized) return false;

    std::lock_guard<std::mutex> lk(m_queueMtx);

    if (static_cast<int>(m_queue.size()) >= m_cfg.queueDepth) {
        if (m_cfg.dropOnFull) {
            m_queue.pop(); // Drop oldest
            ++m_framesDropped;
        } else {
            return false; // Back-pressure
        }
    }
    m_queue.push(std::move(frame));
    m_queueCv.notify_one();
    return true;
}

void UpscalerPipeline::setOutputCallback(OutputCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_outputCb = std::move(cb);
}

void UpscalerPipeline::flush() {
    // Wait until queue drains
    std::unique_lock<std::mutex> lk(m_queueMtx);
    m_queueCv.wait(lk, [this] { return m_queue.empty() || !m_running; });
}

double UpscalerPipeline::avgProcessingMs() const noexcept {
    std::lock_guard<std::mutex> lk(m_statsMtx);
    return (m_statCount > 0) ? (m_totalMs / m_statCount) : 0.0;
}

bool UpscalerPipeline::swapModel(const UpscalerConfig& newCfg) {
    flush();
    std::lock_guard<std::mutex> lk(m_queueMtx);

    if (m_upscaler) {
        m_upscaler->shutdown();
        m_upscaler.reset();
    }

    m_cfg.upscaler  = newCfg;
    m_cfg.passThrough = false;

    m_upscaler = UpscalerFactory::create(newCfg.model);
    if (!m_upscaler->init(newCfg)) {
        m_cfg.passThrough = true;
        return false;
    }
    return true;
}

// ── Worker loop ───────────────────────────────────────────────────────────────
void UpscalerPipeline::workerLoop() {
    while (true) {
        video::VideoFramePtr frame;

        {
            std::unique_lock<std::mutex> lk(m_queueMtx);
            m_queueCv.wait(lk, [this] {
                return !m_queue.empty() || !m_running;
            });

            if (!m_running && m_queue.empty()) break;
            if (m_queue.empty()) continue;

            frame = std::move(m_queue.front());
            m_queue.pop();
        }
        m_queueCv.notify_all(); // unblock flush()

        // ── Process ──────────────────────────────────────────────────────────
        video::VideoFramePtr result;
        auto t0 = std::chrono::steady_clock::now();

        if (m_cfg.passThrough || !m_upscaler) {
            result = frame;
        } else {
            try {
                result = m_upscaler->process(frame);
            } catch (...) {
                result = frame; // on error, forward original
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_totalMs += ms;
            ++m_statCount;
        }
        ++m_framesProcessed;

        // ── Deliver ──────────────────────────────────────────────────────────
        std::lock_guard<std::mutex> lk(m_cbMtx);
        if (m_outputCb && result) m_outputCb(std::move(result));
    }
}

} // namespace aurora::upscaler

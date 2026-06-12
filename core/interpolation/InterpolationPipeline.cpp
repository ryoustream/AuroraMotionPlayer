#include "InterpolationPipeline.h"
#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#  include <windows.h>
#  include <dxgi.h>
#  pragma comment(lib,"dxgi.lib")
#endif
#ifdef __ANDROID__
#  include <sys/system_properties.h>
#endif

namespace aurora::interpolation {

using namespace std::chrono;

// ── Constructor / Destructor ──────────────────────────────────────────────────
InterpolationPipeline::InterpolationPipeline() = default;

InterpolationPipeline::~InterpolationPipeline() {
    shutdown();
}

// ── init ──────────────────────────────────────────────────────────────────────
bool InterpolationPipeline::init(const PipelineConfig& cfg) {
    if (m_running) shutdown();
    m_cfg = cfg;

    // Auto-detect tiling need based on VRAM
    if (!cfg.useTiling) {
        int vram = queryVRAMMB();
        if (vram > 0 && vram < cfg.vramThresholdMB) {
            m_cfg.useTiling = true;
        }
    }

    // Init AuroraFlow (interpolation engine)
    m_flow = std::make_unique<AuroraFlow>();
    if (!m_flow->init(cfg.interp)) {
        m_flow.reset();
        return false;
    }

    // Wire AuroraFlow output callback → our emitFrame
    m_flow->setOutputCallback([this](aurora::video::VideoFramePtr f) {
        emitFrame(std::move(f));
    });

    // Init TileProcessor if needed
    if (m_cfg.useTiling) {
        m_tiler = std::make_unique<TileProcessor>(cfg.tile);
    }

    m_running = true;
    m_flushing = false;

    // Spawn worker thread
    m_worker = std::thread(&InterpolationPipeline::workerLoop, this);

    return true;
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void InterpolationPipeline::shutdown() {
    if (!m_running) return;
    m_running = false;
    m_inputCv.notify_all();

    if (m_worker.joinable()) m_worker.join();

    if (m_flow)  { m_flow->shutdown();  m_flow.reset(); }
    m_tiler.reset();

    // Drain input queue
    {
        std::lock_guard<std::mutex> lk(m_inputMtx);
        while (!m_inputQueue.empty()) m_inputQueue.pop();
    }
}

// ── push ──────────────────────────────────────────────────────────────────────
bool InterpolationPipeline::push(aurora::video::VideoFramePtr frame) {
    if (!m_running || !frame) return false;

    std::unique_lock<std::mutex> lk(m_inputMtx);
    // Non-blocking: drop oldest frame if queue full to avoid stall
    if (m_inputQueue.size() >= m_cfg.inputQueueDepth) {
        m_inputQueue.pop();
        std::lock_guard<std::mutex> sl(m_statsMtx);
        ++m_stats.droppedFrames;
    }
    m_inputQueue.push(std::move(frame));
    lk.unlock();
    m_inputCv.notify_one();

    // Track input FPS
    {
        std::lock_guard<std::mutex> sl(m_statsMtx);
        ++m_stats.framesIn;
        ++m_inputFPSCount;
        auto now = steady_clock::now();
        double elapsed = duration<double>(now - m_lastInputTime).count();
        if (elapsed >= 1.0) {
            m_stats.realInputFPS = (float)(m_inputFPSCount / elapsed);
            m_inputFPSCount = 0;
            m_lastInputTime = now;
        }
    }
    return true;
}

// ── flush ─────────────────────────────────────────────────────────────────────
void InterpolationPipeline::flush() {
    m_flushing = true;
    m_inputCv.notify_all();

    // Wait until queue is drained
    std::unique_lock<std::mutex> lk(m_inputMtx);
    m_inputSpaceCv.wait(lk, [this]{ return m_inputQueue.empty() || !m_running; });

    if (m_flow) m_flow->flush();
    m_flushing = false;
}

// ── setOutputCallback ─────────────────────────────────────────────────────────
void InterpolationPipeline::setOutputCallback(OutputCb cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_outputCb = std::move(cb);
    // Also wire to AuroraFlow so interpolated frames reach us
    if (m_flow) {
        m_flow->setOutputCallback([this](aurora::video::VideoFramePtr f) {
            emitFrame(std::move(f));
        });
    }
}

// ── setPipelineConfig ─────────────────────────────────────────────────────────
void InterpolationPipeline::setPipelineConfig(const PipelineConfig& cfg) {
    m_cfg = cfg;
    if (m_flow) m_flow->setConfig(cfg.interp);
    if (m_tiler) m_tiler->setConfig(cfg.tile);
}

// ── stats ─────────────────────────────────────────────────────────────────────
PipelineStats InterpolationPipeline::stats() const noexcept {
    std::lock_guard<std::mutex> lk(m_statsMtx);
    // Also merge AuroraFlow stats
    if (m_flow) {
        auto fs = m_flow->stats();
        PipelineStats merged = m_stats;
        merged.sceneCuts      = fs.sceneCuts;
        merged.avgInferenceMs = fs.avgInferenceMs;
        return merged;
    }
    return m_stats;
}

// ── workerLoop ────────────────────────────────────────────────────────────────
void InterpolationPipeline::workerLoop() {
    while (m_running || m_flushing) {
        aurora::video::VideoFramePtr frame;

        {
            std::unique_lock<std::mutex> lk(m_inputMtx);
            m_inputCv.wait(lk, [this]{
                return !m_inputQueue.empty() || !m_running || m_flushing;
            });

            if (m_inputQueue.empty()) {
                if (!m_running) break;
                continue;
            }
            frame = std::move(m_inputQueue.front());
            m_inputQueue.pop();
        }
        m_inputSpaceCv.notify_all();

        if (!frame) continue;

        auto t0 = steady_clock::now();

        // Push to AuroraFlow (which handles interpolation + emits via callback)
        if (m_flow) m_flow->push(frame);

        auto t1 = steady_clock::now();
        double ms = duration<double,std::milli>(t1-t0).count();

        {
            std::lock_guard<std::mutex> sl(m_statsMtx);
            m_stats.avgPipelineMs = m_stats.avgPipelineMs * 0.9 + ms * 0.1;
        }
    }
}

// ── emitFrame ─────────────────────────────────────────────────────────────────
void InterpolationPipeline::emitFrame(aurora::video::VideoFramePtr f) {
    if (!f) return;

    {
        std::lock_guard<std::mutex> sl(m_statsMtx);
        ++m_stats.framesOut;
        ++m_outputFPSCount;
        auto now = steady_clock::now();
        double elapsed = duration<double>(now - m_lastOutputTime).count();
        if (elapsed >= 1.0) {
            m_stats.realOutputFPS = (float)(m_outputFPSCount / elapsed);
            m_outputFPSCount = 0;
            m_lastOutputTime = now;
        }
    }

    std::lock_guard<std::mutex> lk(m_cbMtx);
    if (m_outputCb) m_outputCb(std::move(f));
}

// ── queryVRAMMB ───────────────────────────────────────────────────────────────
int InterpolationPipeline::queryVRAMMB() {
#ifdef _WIN32
    IDXGIFactory* factory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory),(void**)&factory))) return -1;
    IDXGIAdapter* adapter = nullptr;
    if (FAILED(factory->EnumAdapters(0, &adapter))) { factory->Release(); return -1; }
    DXGI_ADAPTER_DESC desc{};
    adapter->GetDesc(&desc);
    int mb = (int)(desc.DedicatedVideoMemory / (1024*1024));
    adapter->Release();
    factory->Release();
    return mb;
#else
    // On Linux/Android: read from sysfs or return -1 to be safe
    return -1;
#endif
}

} // namespace aurora::interpolation

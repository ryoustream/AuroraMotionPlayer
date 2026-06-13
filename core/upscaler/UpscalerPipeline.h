#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// UpscalerPipeline.h  —  Aurora Motion Player
// Threaded upscaler pipeline: owns a queue of VideoFrames, processes them
// asynchronously on a dedicated worker thread, delivers upscaled frames
// via a callback.  Integrates with AIPipelineManager.
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerFactory.h"
#include "video/VideoFrame.h"
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <string>

namespace aurora::upscaler {

// ── Pipeline configuration ────────────────────────────────────────────────────
struct PipelineConfig {
    UpscalerConfig  upscaler;          ///< Model + factor + paths
    int             queueDepth   = 4;  ///< Max frames buffered before backpressure
    bool            passThrough  = false; ///< If true, bypass and forward unchanged
    bool            dropOnFull   = true;  ///< Drop oldest frame if queue full
};

// ── Upscaler Pipeline ─────────────────────────────────────────────────────────
class UpscalerPipeline {
public:
    using OutputCallback = std::function<void(video::VideoFramePtr)>;

    explicit UpscalerPipeline();
    ~UpscalerPipeline();

    // Non-copyable / non-movable (owns a thread)
    UpscalerPipeline(const UpscalerPipeline&) = delete;
    UpscalerPipeline& operator=(const UpscalerPipeline&) = delete;

    /// Initialize with config.  Returns false if model load fails.
    bool init(const PipelineConfig& cfg);

    /// Shut down worker thread and release model.
    void shutdown();

    /// Push a frame for processing (non-blocking).
    /// Returns false if queue is full and dropOnFull is false.
    bool pushFrame(video::VideoFramePtr frame);

    /// Register callback invoked from the worker thread with each upscaled frame.
    void setOutputCallback(OutputCallback cb);

    /// Flush pending frames (block until queue is empty).
    void flush();

    // ── Stats ────────────────────────────────────────────────────────────────
    uint64_t framesProcessed()  const noexcept { return m_framesProcessed; }
    uint64_t framesDropped()    const noexcept { return m_framesDropped; }
    double   avgProcessingMs()  const noexcept;
    bool     isInitialized()    const noexcept { return m_initialized; }

    /// Hot-swap model at runtime (drains queue first).
    bool swapModel(const UpscalerConfig& newCfg);

private:
    void workerLoop();

    PipelineConfig                  m_cfg;
    std::unique_ptr<UpscalerBase>   m_upscaler;

    // Worker thread
    std::thread                     m_worker;
    std::atomic<bool>               m_running{false};

    // Frame queue
    std::queue<video::VideoFramePtr> m_queue;
    std::mutex                       m_queueMtx;
    std::condition_variable          m_queueCv;

    OutputCallback                   m_outputCb;
    std::mutex                       m_cbMtx;

    // Stats
    std::atomic<uint64_t>           m_framesProcessed{0};
    std::atomic<uint64_t>           m_framesDropped{0};
    mutable std::mutex              m_statsMtx;
    double                          m_totalMs       = 0.0;
    uint64_t                        m_statCount     = 0;

    bool                            m_initialized   = false;
};

} // namespace aurora::upscaler

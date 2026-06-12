#pragma once
#include "AuroraFlow.h"
#include "TileProcessor.h"
#include "video/VideoFrame.h"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <chrono>

namespace aurora::interpolation {

// Thread-safe pipeline: producer pushes decoded frames, consumer receives
// interpolated frames via callback — all inference runs on a dedicated thread.
struct PipelineConfig {
    InterpolationConfig interp;
    TileConfig          tile;
    size_t              inputQueueDepth  = 4;   // Max buffered input frames
    size_t              outputQueueDepth = 8;   // Max buffered output frames
    bool                useTiling        = false; // Auto-enable for VRAM < 4 GB
    int                 vramThresholdMB  = 3500;  // Below this → tiling
};

struct PipelineStats {
    uint64_t framesIn        = 0;
    uint64_t framesOut       = 0;
    uint64_t droppedFrames   = 0;
    uint64_t sceneCuts       = 0;
    double   avgInferenceMs  = 0.0;
    double   avgPipelineMs   = 0.0;
    float    realInputFPS    = 0.0f;
    float    realOutputFPS   = 0.0f;
};

class InterpolationPipeline {
public:
    using OutputCb = std::function<void(aurora::video::VideoFramePtr)>;

    InterpolationPipeline();
    ~InterpolationPipeline();

    // Initialize pipeline (spawns worker thread)
    bool init(const PipelineConfig& cfg);
    void shutdown();

    // Push a decoded frame (non-blocking; drops if queue full)
    bool push(aurora::video::VideoFramePtr frame);

    // Flush remaining frames (blocking until worker drains)
    void flush();

    void setOutputCallback(OutputCb cb);
    void setPipelineConfig(const PipelineConfig& cfg);

    bool            isRunning()  const noexcept { return m_running; }
    PipelineStats   stats()      const noexcept;

private:
    void workerLoop();
    void processFramePair(aurora::video::VideoFramePtr f0,
                          aurora::video::VideoFramePtr f1);
    void emitFrame(aurora::video::VideoFramePtr f);

    // Estimate available VRAM (MB); returns -1 if unknown
    static int queryVRAMMB();

    PipelineConfig    m_cfg;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_flushing{false};

    // Input queue
    std::queue<aurora::video::VideoFramePtr> m_inputQueue;
    std::mutex              m_inputMtx;
    std::condition_variable m_inputCv;
    std::condition_variable m_inputSpaceCv;

    // Output callback (called from worker thread)
    OutputCb   m_outputCb;
    std::mutex m_cbMtx;

    // Worker thread
    std::thread m_worker;

    // Core interpolation engine
    std::unique_ptr<AuroraFlow>   m_flow;
    std::unique_ptr<TileProcessor> m_tiler;

    // Statistics
    mutable std::mutex m_statsMtx;
    PipelineStats      m_stats;

    // FPS tracking
    std::chrono::steady_clock::time_point m_lastInputTime;
    std::chrono::steady_clock::time_point m_lastOutputTime;
    uint64_t m_inputFPSCount  = 0;
    uint64_t m_outputFPSCount = 0;
};

} // namespace aurora::interpolation

#pragma once
/**
 * Aurora Motion Player — AI Processing Pipeline
 *
 * Orchestrates the full AI processing chain:
 *   Decode → Denoise → Upscale → Frame-Interpolate → Tone-Map → Render
 *
 * Each stage is optional and can be enabled/disabled independently.
 * Stages communicate via VideoFrame queues and run on a dedicated thread pool.
 */

#include "../video/VideoFrame.h"
#include "../hdr/HDREngine.h"
#include "../interpolation/AuroraFlow.h"
#include "../upscaler/UpscalerFactory.h"
#include "../scene/SceneDetector.h"
#include "../benchmark/BenchmarkSystem.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace aurora::core {

// ── Pipeline configuration ────────────────────────────────────────────────────
struct PipelineConfig {
    // Denoise
    bool  denoiseEnabled     = false;
    float denoiseStrength    = 0.3f;   // 0.0 – 1.0

    // Upscale
    bool        upscaleEnabled  = false;
    std::string upscalerName    = "RealESRGAN";
    int         upscaleScale    = 2;    // 2, 4, 8

    // Frame interpolation
    bool        interpEnabled   = false;
    std::string interpModel     = "RIFE";
    float       targetFPS       = 60.0f;
    bool        adaptiveMotion  = true;  // skip interp on scene cuts

    // HDR / Tone mapping
    bool        hdrEnabled      = false;
    std::string toneMappingMode = "BT2390"; // BT2390|Mobius|ACES|Reinhard

    // AI auto-select
    bool        autoSelectModels = true;  // scene-aware model selection

    // Queue sizes
    int         decodeQueueSize = 4;
    int         outputQueueSize = 4;

    // Thread count (0 = auto)
    int         processingThreads = 0;
};

// ── Pipeline statistics ───────────────────────────────────────────────────────
struct PipelineStats {
    float decodeLatencyMs    = 0.0f;
    float denoiseLatencyMs   = 0.0f;
    float upscaleLatencyMs   = 0.0f;
    float interpLatencyMs    = 0.0f;
    float toneMappingLatencyMs = 0.0f;
    float totalLatencyMs     = 0.0f;
    float throughputFPS      = 0.0f;
    int   droppedFrames      = 0;
    int   interpolatedFrames = 0;
};

// ── Pipeline state ────────────────────────────────────────────────────────────
enum class PipelineState { Idle, Running, Paused, Error };

// ── Output callback ───────────────────────────────────────────────────────────
using FrameOutputCb = std::function<void(std::shared_ptr<VideoFrame>)>;

// ── AI Pipeline Manager ───────────────────────────────────────────────────────
class AIPipelineManager {
public:
    explicit AIPipelineManager();
    ~AIPipelineManager();

    // Non-copyable, movable
    AIPipelineManager(const AIPipelineManager&)            = delete;
    AIPipelineManager& operator=(const AIPipelineManager&) = delete;

    // ----- Configuration -----------------------------------------------------
    void configure(const PipelineConfig& config);
    const PipelineConfig& config() const { return m_config; }

    // ----- Lifecycle ---------------------------------------------------------
    bool start();
    void pause();
    void resume();
    void stop();
    void flush();

    PipelineState state() const { return m_state.load(); }

    // ----- Frame input -------------------------------------------------------
    /// Push a decoded frame into the pipeline.
    /// Returns false if the input queue is full (caller should back-pressure).
    bool pushFrame(std::shared_ptr<VideoFrame> frame);

    // ----- Output ------------------------------------------------------------
    void setOutputCallback(FrameOutputCb cb) { m_outputCb = std::move(cb); }

    // ----- Stats -------------------------------------------------------------
    PipelineStats stats() const;

    // ----- Scene-aware auto-config -------------------------------------------
    void onSceneChange();   // Notifies pipeline of scene cut

private:
    void processingLoop();
    std::shared_ptr<VideoFrame> runDenoise(std::shared_ptr<VideoFrame> frame);
    std::shared_ptr<VideoFrame> runUpscale(std::shared_ptr<VideoFrame> frame);
    void                        runInterpolate(std::shared_ptr<VideoFrame> prev,
                                               std::shared_ptr<VideoFrame> curr);
    std::shared_ptr<VideoFrame> runToneMap(std::shared_ptr<VideoFrame> frame);

    // Config & state
    PipelineConfig               m_config;
    std::atomic<PipelineState>   m_state    {PipelineState::Idle};
    std::atomic<bool>            m_sceneChanged {false};

    // Input queue
    mutable std::mutex           m_inputMutex;
    std::condition_variable      m_inputCV;
    std::queue<std::shared_ptr<VideoFrame>> m_inputQueue;

    // Output
    FrameOutputCb                m_outputCb;

    // Processing thread
    std::thread                  m_processThread;

    // Previous frame (for interpolation)
    std::shared_ptr<VideoFrame>  m_prevFrame;

    // Stats (atomic-capable fields)
    mutable std::mutex           m_statsMutex;
    PipelineStats                m_stats;

    // Sub-systems
    std::unique_ptr<HDREngine>       m_hdrEngine;
    std::unique_ptr<AuroraFlow>      m_auroraFlow;
    UpscalerFactory                  m_upscalerFactory;
};

} // namespace aurora::core

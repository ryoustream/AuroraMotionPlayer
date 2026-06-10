#pragma once
#include "video/VideoFrame.h"
#include <memory>
#include <string>
#include <functional>
#include <atomic>

namespace aurora::interpolation {

enum class InterpolationModel {
    RIFE,
    IFRNet,
    FILM,
    GMFlow,
};

enum class InterpolationQuality {
    Fast,       // ~2x faster, slight quality reduction
    Balanced,   // Default
    High,       // Better flow estimation
    Ultra,      // Maximum quality, full resolution
};

enum class InferenceBackend {
    NCNN,
    ONNX,
    TensorRT,
};

struct InterpolationConfig {
    InterpolationModel   model       = InterpolationModel::RIFE;
    InterpolationQuality quality     = InterpolationQuality::Balanced;
    InferenceBackend     backend     = InferenceBackend::NCNN;
    std::string          modelPath;       // Path to .bin/.param or .onnx
    float                targetFPS   = 60.0f;
    float                sourceFPS   = 24.0f;
    bool                 sceneDetect = true;   // Skip interpolation on scene cut
    float                sceneThreshold = 0.15f;
    int                  gpuDeviceId = 0;
    bool                 useTTA      = false;  // Test-time augmentation
    int                  tileSize    = 0;      // 0 = auto; tile for low VRAM
};

// Interpolation statistics
struct InterpolationStats {
    double avgInferenceMs  = 0.0;
    double avgTotalMs      = 0.0;
    uint64_t framesIn      = 0;
    uint64_t framesOut     = 0;
    uint64_t sceneCuts     = 0;
    float    inputFPS      = 0.0f;
    float    outputFPS     = 0.0f;
};

class AuroraFlow {
public:
    using FrameCallback = std::function<void(aurora::video::VideoFramePtr)>;

    AuroraFlow();
    ~AuroraFlow();

    bool init(const InterpolationConfig& cfg);
    void shutdown();

    // Push a decoded frame; interpolated frames are emitted via callback
    void push(aurora::video::VideoFramePtr frame);
    void flush();

    void setOutputCallback(FrameCallback cb) { m_outputCb = std::move(cb); }
    void setConfig(const InterpolationConfig& cfg);

    bool             isRunning()   const noexcept { return m_running; }
    InterpolationStats stats()     const noexcept { return m_stats; }
    float            inputFPS()    const noexcept;
    float            outputFPS()   const noexcept;

    // Model management
    static std::vector<std::string> availableModels(const std::string& modelDir);
    bool loadModel(const std::string& path);

private:
    void interpolateFrames(aurora::video::VideoFramePtr f0,
                           aurora::video::VideoFramePtr f1,
                           float timestep);
    bool detectSceneCut(aurora::video::VideoFramePtr f0,
                        aurora::video::VideoFramePtr f1);
    float computeMultiplier() const noexcept;

    InterpolationConfig  m_cfg;
    std::atomic<bool>    m_running{false};
    FrameCallback        m_outputCb;
    InterpolationStats   m_stats;

    aurora::video::VideoFramePtr m_prevFrame;

    // Backend-specific implementation (PIMPL)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace aurora::interpolation

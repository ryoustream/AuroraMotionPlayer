#pragma once
#include "video/VideoFrame.h"
#include <memory>
#include <string>

namespace aurora::upscaler {

enum class UpscaleModel {
    RealESRGAN,
    SPAN,
    Anime4K,
    FSRCNN,
};

enum class UpscaleFactor {
    X2 = 2,
    X4 = 4,
    X8 = 8,
};

struct UpscalerConfig {
    UpscaleModel model       = UpscaleModel::RealESRGAN;
    UpscaleFactor factor     = UpscaleFactor::X2;
    std::string   modelPath;
    bool          denoise    = false;
    float         denoiseStrength = 0.5f;
    int           gpuDeviceId    = 0;
    int           tileSize       = 0;   // 0 = auto
};

class UpscalerBase {
public:
    virtual ~UpscalerBase() = default;
    virtual bool init(const UpscalerConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual video::VideoFramePtr process(video::VideoFramePtr frame) = 0;
    virtual bool isInitialized() const noexcept = 0;
};

// ── RealESRGAN ────────────────────────────────────────────────────────────────
class RealESRGAN : public UpscalerBase {
public:
    RealESRGAN();  ~RealESRGAN() override;
    bool init(const UpscalerConfig& cfg) override;
    void shutdown() override;
    video::VideoFramePtr process(video::VideoFramePtr frame) override;
    bool isInitialized() const noexcept override { return m_initialized; }
private:
    std::vector<float> runInference(const std::vector<float>& rgb,
                                     int w, int h, int outW, int outH) const;
    struct Impl; std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
    UpscalerConfig m_cfg;
};

// ── SPAN ──────────────────────────────────────────────────────────────────────
class SPAN : public UpscalerBase {
public:
    SPAN();  ~SPAN() override;
    bool init(const UpscalerConfig& cfg) override;
    void shutdown() override;
    video::VideoFramePtr process(video::VideoFramePtr frame) override;
    bool isInitialized() const noexcept override { return m_initialized; }
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};

// ── Anime4K ───────────────────────────────────────────────────────────────────
class Anime4K : public UpscalerBase {
public:
    Anime4K();  ~Anime4K() override;
    bool init(const UpscalerConfig& cfg) override;
    void shutdown() override;
    video::VideoFramePtr process(video::VideoFramePtr frame) override;
    bool isInitialized() const noexcept override { return m_initialized; }
private:
    bool m_initialized = false;
    UpscalerConfig m_cfg;
};

// ── FSRCNN ────────────────────────────────────────────────────────────────────
class FSRCNN : public UpscalerBase {
public:
    FSRCNN();  ~FSRCNN() override;
    bool init(const UpscalerConfig& cfg) override;
    void shutdown() override;
    video::VideoFramePtr process(video::VideoFramePtr frame) override;
    bool isInitialized() const noexcept override { return m_initialized; }
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};

// ── Factory ───────────────────────────────────────────────────────────────────
class UpscalerFactory {
public:
    static std::unique_ptr<UpscalerBase> create(UpscaleModel model);
};

} // namespace aurora::upscaler

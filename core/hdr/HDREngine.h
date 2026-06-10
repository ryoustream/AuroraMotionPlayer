#pragma once
#include "video/VideoFrame.h"
#include <memory>

namespace aurora::hdr {

enum class HDRFormat {
    None,
    HDR10,
    HDR10Plus,
    HLG,
    DolbyVision,
};

enum class ToneMappingAlgorithm {
    BT2390,   // EETF from BT.2390 — recommended for displays
    Mobius,   // Soft clip with halo avoidance
    ACES,     // Academy Color Encoding System
    Reinhard, // Classic Reinhard
    Linear,   // Simple linear scale
};

struct DisplayCapabilities {
    float maxLuminance    = 400.0f;  // nits, SDR display default
    float minLuminance    = 0.005f;
    float peakWhite       = 203.0f;  // Reference white for SDR
    bool  isHDRCapable    = false;
    float maxFrameAvgLum  = 0.0f;
};

struct HDRMetadata {
    HDRFormat format             = HDRFormat::None;
    float     masterMaxLum       = 1000.0f;   // nits
    float     masterMinLum       = 0.001f;
    float     maxContentLum      = 1000.0f;
    float     maxFrameAvgLum     = 400.0f;
    // Primaries
    float     primaryRx = 0.708f, primaryRy = 0.292f;
    float     primaryGx = 0.170f, primaryGy = 0.797f;
    float     primaryBx = 0.131f, primaryBy = 0.046f;
    float     whiteX    = 0.3127f, whiteY   = 0.3290f;
};

class ToneMapper {
public:
    explicit ToneMapper(ToneMappingAlgorithm algo = ToneMappingAlgorithm::BT2390);

    // Apply tone mapping to a single pixel (linear light, normalized [0,1])
    float map(float x, float srcPeak, float dstPeak) const noexcept;

    void setAlgorithm(ToneMappingAlgorithm algo) { m_algo = algo; }
    ToneMappingAlgorithm algorithm() const noexcept { return m_algo; }

private:
    float bt2390(float x, float srcPeak, float dstPeak) const noexcept;
    float mobius(float x, float srcPeak, float dstPeak) const noexcept;
    float aces(float x) const noexcept;
    float reinhard(float x, float srcPeak, float dstPeak) const noexcept;

    ToneMappingAlgorithm m_algo;
};

class HDREngine {
public:
    HDREngine();
    ~HDREngine() = default;

    bool init(const DisplayCapabilities& display);

    // Process frame: applies PQ/HLG EOTF + tone mapping if needed
    video::VideoFramePtr process(video::VideoFramePtr frame,
                                  const HDRMetadata& meta);

    void setToneMapper(ToneMappingAlgorithm algo);
    void setDisplayCapabilities(const DisplayCapabilities& caps);

    HDRFormat detectFormat(const video::VideoFrame& frame) const;

private:
    DisplayCapabilities  m_display;
    ToneMapper           m_toneMapper;
    bool                 m_initialized = false;

    // PQ (SMPTE ST 2084) transfer function
    static float pqEOTF(float x) noexcept;  // PQ → linear light
    static float pqOETF(float x) noexcept;  // linear → PQ
    static float hlgEOTF(float x) noexcept;
    static float hlgOETF(float x) noexcept;
};

} // namespace aurora::hdr

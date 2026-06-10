#pragma once
#include "video/VideoFrame.h"
#include <memory>
#include <string>

namespace aurora::interpolation {

struct InterpolationConfig;

// RIFE (Real-Time Intermediate Flow Estimation) interpolator
// Paper: https://arxiv.org/abs/2011.06294
// Uses NCNN or ONNX Runtime for inference
class RIFEInterpolator {
public:
    RIFEInterpolator();
    ~RIFEInterpolator();

    bool init(const InterpolationConfig& cfg);
    void shutdown();

    video::VideoFramePtr interpolate(video::VideoFramePtr f0,
                                     video::VideoFramePtr f1,
                                     float timestep);

    bool isInitialized() const noexcept { return m_initialized; }

private:
    // Preprocessing: convert YUV → RGB float tensor
    void yuvToRGBFloat(video::VideoFramePtr frame,
                       std::vector<float>& out,
                       int& outW, int& outH);

    // Postprocessing: convert RGB float tensor → YUV VideoFrame
    video::VideoFramePtr rgbFloatToFrame(const std::vector<float>& rgb,
                                          int width, int height,
                                          const video::VideoFrame& ref);

    // Pad to multiple of 32 (RIFE requirement)
    int padTo32(int val) const noexcept;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool                  m_initialized = false;

    int   m_padW  = 0;
    int   m_padH  = 0;
    bool  m_useTTA = false;
    int   m_tileSize = 0;
};

} // namespace aurora::interpolation

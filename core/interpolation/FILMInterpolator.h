#pragma once
#include "video/VideoFrame.h"
#include <memory>

namespace aurora::interpolation {
struct InterpolationConfig;

// FILM: Frame Interpolation for Large Motion
// Paper: https://arxiv.org/abs/2202.04901
class FILMInterpolator {
public:
    FILMInterpolator();
    ~FILMInterpolator();
    bool init(const InterpolationConfig& cfg);
    void shutdown();
    video::VideoFramePtr interpolate(video::VideoFramePtr f0,
                                     video::VideoFramePtr f1,
                                     float t);
    bool isInitialized() const noexcept { return m_initialized; }
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};
} // namespace aurora::interpolation

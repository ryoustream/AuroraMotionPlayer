#pragma once
#include "video/VideoFrame.h"
#include <memory>
#include <vector>

namespace aurora::interpolation {

// Motion vector for a single macroblock
struct MotionVector {
    int16_t dx = 0;  // horizontal displacement (pixels)
    int16_t dy = 0;  // vertical displacement
    float   sad = 0.0f;  // sum of absolute differences (confidence)
};

class MotionCompensator {
public:
    MotionCompensator() = default;
    ~MotionCompensator() = default;

    // Apply motion compensation using motion vectors to produce intermediate frame
    // between f0 and f1 at fractional timestep t in [0,1]
    video::VideoFramePtr compensate(
        const video::VideoFrame& f0,
        const video::VideoFrame& f1,
        const std::vector<MotionVector>& mvs,
        float t,
        int blockSize = 16) const;

    // Bilateral compensation: average forward + backward warped frames
    video::VideoFramePtr compensateBilateral(
        const video::VideoFrame& f0,
        const video::VideoFrame& f1,
        const std::vector<MotionVector>& forwardMVs,
        const std::vector<MotionVector>& backwardMVs,
        float t,
        int blockSize = 16) const;

private:
    // Bilinear sample from plane data
    static float sampleBilinear(const uint8_t* plane, int stride,
                                 int w, int h, float x, float y) noexcept;
};

} // namespace aurora::interpolation

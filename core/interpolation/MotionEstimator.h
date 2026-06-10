#pragma once
#include "video/VideoFrame.h"
#include <vector>

namespace aurora::interpolation {

struct MotionVector {
    int16_t dx = 0;
    int16_t dy = 0;
    float   confidence = 0.0f;
};

using MotionField = std::vector<MotionVector>;

enum class MEAlgorithm {
    BlockMatching,   // Fast, hardware-friendly
    OpticalFlow,     // Dense Farneback
    GMFlow,          // Global Motion Flow (neural)
};

struct MEConfig {
    MEAlgorithm algorithm  = MEAlgorithm::BlockMatching;
    int         blockSize  = 16;    // For block matching
    int         searchRange = 32;
    int         levels     = 4;    // Pyramid levels for optical flow
};

class MotionEstimator {
public:
    explicit MotionEstimator(MEConfig cfg = {});
    ~MotionEstimator() = default;

    // Compute dense motion field from f0 → f1
    MotionField estimate(video::VideoFramePtr f0, video::VideoFramePtr f1);

    void setConfig(const MEConfig& cfg) { m_cfg = cfg; }

private:
    MotionField blockMatching(video::VideoFramePtr f0, video::VideoFramePtr f1);
    MotionField opticalFlow(video::VideoFramePtr f0, video::VideoFramePtr f1);

    MEConfig m_cfg;
};

class MotionCompensator {
public:
    // Apply motion field to warp frame f0 toward f1 at timestep t
    video::VideoFramePtr compensate(video::VideoFramePtr f0,
                                    const MotionField& field,
                                    int fieldW, int fieldH,
                                    float t);
};

} // namespace aurora::interpolation

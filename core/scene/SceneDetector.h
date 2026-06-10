#pragma once
#include "video/VideoFrame.h"
#include <string>
#include <vector>

namespace aurora::scene {

enum class ContentType {
    Unknown,
    Movie,
    Anime,
    LiveAction,
    Sports,
    Gaming,
    Animation,
};

struct SceneAnalysis {
    ContentType    contentType      = ContentType::Unknown;
    bool           isSceneCut       = false;
    float          sceneChangeScore = 0.0f;  // 0-1
    float          motionIntensity  = 0.0f;  // 0-1
    float          colorfulness     = 0.0f;  // 0-1
    float          edgeDensity      = 0.0f;  // 0-1 (anime tends high)
    std::string    recommendedInterpolation;  // e.g. "RIFE", "FILM"
    std::string    recommendedUpscaler;       // e.g. "Anime4K", "RealESRGAN"
    std::string    recommendedDenoiser;
};

class SceneDetector {
public:
    SceneDetector();
    ~SceneDetector() = default;

    SceneAnalysis analyze(video::VideoFramePtr prev, video::VideoFramePtr curr);

    void setSceneThreshold(float t) noexcept { m_sceneThreshold = t; }
    float sceneThreshold() const noexcept { return m_sceneThreshold; }

private:
    float computeSAD(video::VideoFramePtr a, video::VideoFramePtr b) const;
    float computeEdgeDensity(video::VideoFramePtr frame) const;
    float computeMotionIntensity(video::VideoFramePtr a, video::VideoFramePtr b) const;
    ContentType classifyContent(const SceneAnalysis& partial) const;

    float m_sceneThreshold = 0.35f;
};

} // namespace aurora::scene

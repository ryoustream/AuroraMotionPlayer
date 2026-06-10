#include "SceneDetector.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace aurora::scene {

SceneDetector::SceneDetector() = default;

float SceneDetector::computeSAD(video::VideoFramePtr a, video::VideoFramePtr b) const {
    if (!a || !b) return 0.0f;
    const uint8_t* y0 = a->data(0);
    const uint8_t* y1 = b->data(0);
    if (!y0 || !y1) return 0.0f;

    int w = std::min(a->width(),  b->width());
    int h = std::min(a->height(), b->height());
    int step = std::max(1, (w * h) / 8192);
    int ls0 = a->linesize(0), ls1 = b->linesize(0);

    double sad = 0.0;
    int cnt = 0;
    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            sad += std::abs(static_cast<int>(y0[y*ls0+x]) - static_cast<int>(y1[y*ls1+x]));
            ++cnt;
        }
    }
    return cnt > 0 ? static_cast<float>(sad / cnt / 255.0) : 0.0f;
}

float SceneDetector::computeEdgeDensity(video::VideoFramePtr frame) const {
    if (!frame) return 0.0f;
    const uint8_t* y = frame->data(0);
    if (!y) return 0.0f;

    int w = frame->width(), h = frame->height();
    int ls = frame->linesize(0);
    int step = std::max(1, (w * h) / 8192);
    int edges = 0, cnt = 0;

    // Simple Sobel edge detection
    for (int row = 1; row < h - 1; row += step) {
        for (int col = 1; col < w - 1; col += step) {
            int gx = -y[(row-1)*ls+(col-1)] + y[(row-1)*ls+(col+1)]
                     -2*y[row*ls+(col-1)]   + 2*y[row*ls+(col+1)]
                     -y[(row+1)*ls+(col-1)] + y[(row+1)*ls+(col+1)];
            int gy = -y[(row-1)*ls+(col-1)] - 2*y[(row-1)*ls+col] - y[(row-1)*ls+(col+1)]
                     +y[(row+1)*ls+(col-1)] + 2*y[(row+1)*ls+col] + y[(row+1)*ls+(col+1)];
            if (std::sqrt(gx*gx + gy*gy) > 30) ++edges;
            ++cnt;
        }
    }
    return cnt > 0 ? static_cast<float>(edges) / cnt : 0.0f;
}

float SceneDetector::computeMotionIntensity(video::VideoFramePtr a,
                                             video::VideoFramePtr b) const {
    return computeSAD(a, b);  // SAD is a proxy for motion intensity
}

ContentType SceneDetector::classifyContent(const SceneAnalysis& s) const {
    // Heuristic classification:
    // High edge density + low motion → Anime
    // High motion + high colorfulness → Sports/Gaming
    // Mid range → Movie/Live Action
    if (s.edgeDensity > 0.25f && s.motionIntensity < 0.3f)
        return ContentType::Anime;
    if (s.motionIntensity > 0.5f && s.colorfulness > 0.4f)
        return ContentType::Sports;
    if (s.motionIntensity > 0.6f)
        return ContentType::Gaming;
    if (s.edgeDensity < 0.15f && s.motionIntensity < 0.2f)
        return ContentType::Movie;
    return ContentType::LiveAction;
}

SceneAnalysis SceneDetector::analyze(video::VideoFramePtr prev,
                                      video::VideoFramePtr curr) {
    SceneAnalysis result;
    result.sceneChangeScore = computeSAD(prev, curr);
    result.isSceneCut       = result.sceneChangeScore > m_sceneThreshold;
    result.motionIntensity  = computeMotionIntensity(prev, curr);
    result.edgeDensity      = computeEdgeDensity(curr);
    result.contentType      = classifyContent(result);

    // Auto-select best processing pipeline
    switch (result.contentType) {
    case ContentType::Anime:
        result.recommendedInterpolation = "RIFE";
        result.recommendedUpscaler      = "Anime4K";
        result.recommendedDenoiser      = "light";
        break;
    case ContentType::Sports:
    case ContentType::Gaming:
        result.recommendedInterpolation = "RIFE";
        result.recommendedUpscaler      = "SPAN";
        result.recommendedDenoiser      = "none";
        break;
    case ContentType::Movie:
    case ContentType::LiveAction:
        result.recommendedInterpolation = "FILM";
        result.recommendedUpscaler      = "RealESRGAN";
        result.recommendedDenoiser      = "medium";
        break;
    default:
        result.recommendedInterpolation = "RIFE";
        result.recommendedUpscaler      = "RealESRGAN";
        result.recommendedDenoiser      = "light";
        break;
    }

    return result;
}

} // namespace aurora::scene

#include <cmath>
#include <algorithm>
#include "SceneDetector.h"

// SceneClassifier is part of SceneDetector module
// Additional neural-based classification can be added here as a separate class

namespace aurora::scene {

// This file provides the extended scene classification logic that can
// optionally use an NCNN/ONNX model for more accurate anime/live-action
// classification beyond the heuristic approach in SceneDetector.

class SceneClassifier {
public:
    SceneClassifier() = default;

    // Classify content type using histogram and edge analysis
    ContentType classifyHeuristic(
        float edgeDensity,
        float motionIntensity,
        float colorfulness) const noexcept
    {
        // Decision tree based on visual features
        // Anime: high edge density, moderate colors, lower motion
        if (edgeDensity > 0.20f && colorfulness > 0.3f && motionIntensity < 0.4f)
            return ContentType::Anime;

        // Sports: very high motion, high colorfulness
        if (motionIntensity > 0.55f && colorfulness > 0.35f)
            return ContentType::Sports;

        // Gaming: high motion + high edge density (UI elements)
        if (motionIntensity > 0.45f && edgeDensity > 0.18f)
            return ContentType::Gaming;

        // Animation (non-anime): moderate edges, low motion, vibrant colors
        if (edgeDensity > 0.12f && colorfulness > 0.45f && motionIntensity < 0.25f)
            return ContentType::Animation;

        // Movie/cinematic: low edge density, controlled motion
        if (edgeDensity < 0.10f && motionIntensity < 0.30f)
            return ContentType::Movie;

        return ContentType::LiveAction;
    }

    // Compute colorfulness metric (Hasler & Süsstrunk 2003)
    float computeColorfulness(aurora::video::VideoFramePtr frame) const {
        if (!frame) return 0.0f;
        const uint8_t* cb = frame->data(1);
        const uint8_t* cr = frame->data(2);
        if (!cb || !cr) return 0.0f;

        int w = frame->width() / 2, h = frame->height() / 2;
        int ls = frame->linesize(1);
        int step = std::max(1, (w * h) / 2048);

        double sumRg = 0, sumYb = 0, sumRg2 = 0, sumYb2 = 0;
        int cnt = 0;

        for (int y = 0; y < h; y += step) {
            for (int x = 0; x < w; x += step) {
                float cbv = (cb[y*ls+x] - 128) / 128.0f;
                float crv = (cr[y*ls+x] - 128) / 128.0f;
                sumRg  += crv;
                sumYb  += cbv;
                sumRg2 += crv * crv;
                sumYb2 += cbv * cbv;
                ++cnt;
            }
        }
        if (cnt == 0) return 0.0f;
        double meanRg = sumRg / cnt, meanYb = sumYb / cnt;
        double stdRg  = std::sqrt(sumRg2 / cnt - meanRg * meanRg);
        double stdYb  = std::sqrt(sumYb2 / cnt - meanYb * meanYb);
        double colorfulness = std::sqrt(stdRg*stdRg + stdYb*stdYb) +
                              0.3f * std::sqrt(meanRg*meanRg + meanYb*meanYb);
        return static_cast<float>(std::clamp(colorfulness, 0.0, 1.0));
    }
};

} // namespace aurora::scene

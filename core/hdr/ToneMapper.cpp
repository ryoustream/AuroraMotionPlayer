#include "HDREngine.h"
#include <cmath>
#include <algorithm>

namespace aurora::hdr {

// ── ToneMapper ────────────────────────────────────────────────────────────────

ToneMapper::ToneMapper(ToneMappingAlgorithm algo) : m_algo(algo) {}

float ToneMapper::map(float x, float srcPeak, float dstPeak) const noexcept {
    if (x <= 0.0f) return 0.0f;
    switch (m_algo) {
        case ToneMappingAlgorithm::BT2390:
            return bt2390(x, srcPeak, dstPeak);
        case ToneMappingAlgorithm::Mobius:
            return mobius(x, srcPeak, dstPeak);
        case ToneMappingAlgorithm::ACES:
            return aces(x);
        case ToneMappingAlgorithm::Reinhard:
            return reinhard(x, srcPeak, dstPeak);
        case ToneMappingAlgorithm::Linear:
        default:
            return std::min(x * dstPeak / srcPeak, dstPeak);
    }
}

// BT.2390 Electro-Optical Transfer Function (EETF)
// Reference: ITU-R BT.2390-8 Section 5.4
float ToneMapper::bt2390(float x, float srcPeak, float dstPeak) const noexcept {
    // Normalise input to [0, 1] relative to source peak
    float xn = x / srcPeak;
    if (xn <= 0.0f) return 0.0f;

    float minLum = 0.0f;  // min display luminance (normalised)
    float maxLum = dstPeak / srcPeak;  // max display luminance (normalised)

    // Knee function
    float ks = 1.5f * maxLum - 0.5f;
    if (xn < ks) {
        // Linear region
        return std::min(xn * srcPeak, dstPeak);
    }

    // Hermite spline in the roll-off region
    float t = (xn - ks) / (1.0f - ks);
    float t2 = t * t;
    float t3 = t2 * t;
    float p = (2.0f * t3 - 3.0f * t2 + 1.0f) * ks
            + (t3 - 2.0f * t2 + t) * (1.0f - ks)
            + (-2.0f * t3 + 3.0f * t2) * maxLum;
    return std::clamp(p * srcPeak, 0.0f, dstPeak);
}

// Mobius tone mapping — soft clip with configurable knee
float ToneMapper::mobius(float x, float srcPeak, float dstPeak) const noexcept {
    float scale  = dstPeak / srcPeak;
    float offset = 0.3f;  // transition point
    if (x < offset) return x * scale;

    float a = -offset * offset * (scale - dstPeak / srcPeak) /
              (offset * offset + 2.0f * offset * (dstPeak / srcPeak - scale)
               - 3.0f * (dstPeak / srcPeak) + dstPeak / srcPeak);
    float b = (dstPeak / srcPeak * offset * offset
               - 2.0f * dstPeak / srcPeak * offset + dstPeak / srcPeak) /
              (offset * offset - 2.0f * offset + 1.0f);
    return std::min((a * x + b) / (x + a / (dstPeak / srcPeak)), dstPeak);
}

// ACES filmic tone map (approximation by Krzysztof Narkowicz)
float ToneMapper::aces(float x) const noexcept {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

// Classic Reinhard
float ToneMapper::reinhard(float x, float srcPeak, float dstPeak) const noexcept {
    float xn = x / srcPeak;
    float mapped = xn / (1.0f + xn);
    return mapped * dstPeak;
}

} // namespace aurora::hdr

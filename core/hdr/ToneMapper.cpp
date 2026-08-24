#include "HDREngine.h"
#include <cmath>
#include <algorithm>

namespace aurora::hdr {

// ── ToneMapper ────────────────────────────────────────────────────────────────

ToneMapper::ToneMapper(ToneMappingAlgorithm algo) : m_algo(algo) {}

// map() always returns a value normalised to [0, 1] relative to dstPeak —
// i.e. 1.0 represents full white at the destination display's peak brightness.
// This is the contract every algorithm below must honour.
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
            return std::clamp(x / srcPeak, 0.0f, 1.0f);
    }
}

// BT.2390 Electro-Optical Transfer Function (EETF)
// Reference: ITU-R BT.2390-8 Section 5.4
// Operates entirely in ratio-space relative to srcPeak, then rescales the
// result into [0, 1] relative to dstPeak before returning.
float ToneMapper::bt2390(float x, float srcPeak, float dstPeak) const noexcept {
    float xn = x / srcPeak;   // input normalised to source peak
    if (xn <= 0.0f) return 0.0f;

    float maxLum = dstPeak / srcPeak;  // destination peak, normalised to source peak
    if (maxLum <= 0.0f) return 0.0f;

    // Knee function
    float ks = 1.5f * maxLum - 0.5f;
    float y;
    if (xn < ks) {
        // Linear region — stays in source-normalised space
        y = xn;
    } else {
        // Hermite spline in the roll-off region
        float t  = (xn - ks) / std::max(1.0f - ks, 1e-6f);
        float t2 = t * t;
        float t3 = t2 * t;
        y = (2.0f * t3 - 3.0f * t2 + 1.0f) * ks
          + (t3 - 2.0f * t2 + t) * (1.0f - ks)
          + (-2.0f * t3 + 3.0f * t2) * maxLum;
    }
    y = std::clamp(y, 0.0f, maxLum);
    // Rescale from "source-normalised" space into [0,1] relative to dstPeak
    return std::clamp(y / maxLum, 0.0f, 1.0f);
}

// Mobius tone mapping — soft clip with configurable knee.
// Curve is built in source-normalised space (matches BT2390 above) then
// rescaled into [0,1] relative to dstPeak.
float ToneMapper::mobius(float x, float srcPeak, float dstPeak) const noexcept {
    float xn = x / srcPeak;
    if (xn <= 0.0f) return 0.0f;

    float maxLum = dstPeak / srcPeak;
    if (maxLum <= 0.0f) return 0.0f;

    float j = 0.3f * maxLum;  // knee: transition point in source-normalised space
    float y;
    if (xn <= j) {
        y = xn;
    } else {
        // Solve a,b so the curve is continuous and differentiable at the knee,
        // and asymptotically approaches maxLum as xn → ∞.
        float denom = std::max(maxLum - j, 1e-6f);
        float a = -j * j * (maxLum - j) / std::max(j * j - 2.0f * j * maxLum + maxLum, 1e-6f);
        float b = (j * j - 2.0f * j * maxLum + maxLum) / denom;
        y = (b * b + 2.0f * b * j + j * j) / std::max(b + xn - j, 1e-6f) + maxLum - b - j;
    }
    y = std::clamp(y, 0.0f, maxLum);
    return std::clamp(y / maxLum, 0.0f, 1.0f);
}

// ACES filmic tone map (approximation by Krzysztof Narkowicz)
// Already produces a normalised [0,1] result independent of dstPeak.
float ToneMapper::aces(float x) const noexcept {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

// Classic Reinhard — xn/(1+xn) is already bounded to [0,1) as xn → ∞,
// so no extra rescale by dstPeak is needed (or correct, since multiplying
// by dstPeak would push the result back into raw-nits range).
float ToneMapper::reinhard(float x, float srcPeak, float dstPeak) const noexcept {
    (void)dstPeak;
    float xn = x / srcPeak;
    float mapped = xn / (1.0f + xn);
    return std::clamp(mapped, 0.0f, 1.0f);
}

} // namespace aurora::hdr

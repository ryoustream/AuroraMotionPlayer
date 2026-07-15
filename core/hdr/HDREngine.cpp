#include <cstring>
#include "HDREngine.h"
#include <cmath>
#include <algorithm>

namespace aurora::hdr {

// ── PQ / HLG Transfer Functions ───────────────────────────────────────────────
// SMPTE ST 2084 (PQ) inverse EOTF: PQ code → linear light (cd/m²)
float HDREngine::pqEOTF(float x) noexcept {
    constexpr float m1 = 0.1593017578125f;
    constexpr float m2 = 78.84375f;
    constexpr float c1 = 0.8359375f;
    constexpr float c2 = 18.8515625f;
    constexpr float c3 = 18.6875f;

    float xm1 = std::pow(std::max(x, 0.0f), 1.0f / m2);
    float num  = std::max(xm1 - c1, 0.0f);
    float den  = c2 - c3 * xm1;
    return std::pow(num / den, 1.0f / m1) * 10000.0f; // result in nits
}

float HDREngine::pqOETF(float x) noexcept {
    // x in nits → PQ code
    constexpr float m1 = 0.1593017578125f;
    constexpr float m2 = 78.84375f;
    constexpr float c1 = 0.8359375f;
    constexpr float c2 = 18.8515625f;
    constexpr float c3 = 18.6875f;

    float xn = x / 10000.0f;
    float xm1 = std::pow(xn, m1);
    float num = c1 + c2 * xm1;
    float den = 1.0f + c3 * xm1;
    return std::pow(num / den, m2);
}

// ARIB STD-B67 (HLG) EOTF
float HDREngine::hlgEOTF(float x) noexcept {
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    if (x <= 0.5f)
        return (x * x) / 3.0f;
    else
        return (std::exp((x - c) / a) + b) / 12.0f;
}

float HDREngine::hlgOETF(float x) noexcept {
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    if (x <= 1.0f / 12.0f)
        return std::sqrt(3.0f * x);
    else
        return a * std::log(12.0f * x - b) + c;
}

// ── ToneMapper implementation moved to ToneMapper.cpp (was duplicated here) ──


// ── HDREngine ─────────────────────────────────────────────────────────────────
HDREngine::HDREngine()
    : m_toneMapper(ToneMappingAlgorithm::BT2390)
{}

bool HDREngine::init(const DisplayCapabilities& display) {
    m_display     = display;
    m_initialized = true;
    return true;
}

void HDREngine::setToneMapper(ToneMappingAlgorithm algo) {
    m_toneMapper.setAlgorithm(algo);
}

void HDREngine::setDisplayCapabilities(const DisplayCapabilities& caps) {
    m_display = caps;
}

HDRFormat HDREngine::detectFormat(const video::VideoFrame& frame) const {
    auto meta = frame.colorMeta();
    if (!meta.isHDR) return HDRFormat::None;
    if (meta.transfer == video::TransferFunction::SMPTE2084) return HDRFormat::HDR10;
    if (meta.transfer == video::TransferFunction::ARIB_STD_B67) return HDRFormat::HLG;
    return HDRFormat::None;
}

video::VideoFramePtr HDREngine::process(video::VideoFramePtr frame,
                                         const HDRMetadata& meta)
{
    if (!m_initialized || !frame) return frame;

    // If display is HDR-capable and content is HDR — pass through
    if (m_display.isHDRCapable && meta.format != HDRFormat::None) {
        return frame;
    }

    // Otherwise, tone-map HDR → SDR
    // In a full GPU-accelerated implementation this runs as a Vulkan/OpenGL shader
    // CPU path below for correctness reference
    if (meta.format == HDRFormat::None) return frame;

    auto out = std::make_shared<video::VideoFrame>(
        frame->width(), frame->height(), video::PixelFormat::YUV420P);

    float srcPeak = meta.masterMaxLum;
    float dstPeak = m_display.maxLuminance;

    const uint8_t* srcY = frame->data(0);
    uint8_t*       dstY = out->data(0);
    int ls = frame->linesize(0);
    int ols = out->linesize(0);

    for (int y = 0; y < frame->height(); ++y) {
        for (int x = 0; x < frame->width(); ++x) {
            float pq = srcY[y * ls + x] / 255.0f;
            float lin = pqEOTF(pq);          // → nits
            float tm  = m_toneMapper.map(lin, srcPeak, dstPeak); // → [0,1]
            // BT.709 OETF (gamma 2.2 approximation)
            float sdr = std::pow(std::max(tm, 0.0f), 1.0f / 2.2f);
            dstY[y * ols + x] = static_cast<uint8_t>(
                std::clamp(sdr * 235.0f + 16.0f, 16.0f, 235.0f));
        }
    }

    // Copy chroma planes as-is (chrominance tone mapping is more complex)
    for (int p = 1; p <= 2; ++p) {
        if (!frame->data(p) || !out->data(p)) continue;
        int ch = frame->height() / 2;
        for (int y = 0; y < ch; ++y) {
            memcpy(out->data(p) + y * out->linesize(p),
                   frame->data(p) + y * frame->linesize(p),
                   frame->linesize(p));
        }
    }

    auto outMeta = frame->colorMeta();
    outMeta.isHDR    = false;
    outMeta.transfer = video::TransferFunction::BT709;
    out->setColorMeta(outMeta);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    return out;
}

} // namespace aurora::hdr

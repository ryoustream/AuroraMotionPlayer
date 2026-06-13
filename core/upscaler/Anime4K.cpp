// ─────────────────────────────────────────────────────────────────────────────
// Anime4K.cpp  —  Aurora Motion Player
// Anime4K — real-time anime upscaling via shader-based edge enhancement.
// Reference: https://github.com/bloc97/Anime4K
// This implementation provides a CPU-side pre-/post-processing pass; the
// actual Anime4K Vulkan/GLSL shaders are applied by the renderer.
// For offline/CPU-only mode we use a guided-upsampling fallback.
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerFactory.h"
#include "ImageUtils.h"
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#  include <ncnn/mat.h>
#endif

namespace aurora::upscaler {

// ── Anime4K Push shader approximation (CPU) ──────────────────────────────────
// Implements a simplified version of Anime4K PUSH+UPSCALE on the CPU.
// In GPU-accelerated mode, the renderer uses Vulkan compute shaders directly.
namespace {

// Compute Sobel edge strength at (x,y) on a single-channel image
float sobelEdge(const std::vector<float>& ch, int w, int h, int x, int y) {
    auto get = [&](int px, int py) -> float {
        return ch[std::clamp(py, 0, h-1) * w + std::clamp(px, 0, w-1)];
    };
    float gx = -get(x-1,y-1) + get(x+1,y-1)
               -2*get(x-1,y) + 2*get(x+1,y)
               -get(x-1,y+1) + get(x+1,y+1);
    float gy = -get(x-1,y-1) - 2*get(x,y-1) - get(x+1,y-1)
               +get(x-1,y+1) + 2*get(x,y+1) + get(x+1,y+1);
    return std::sqrt(gx*gx + gy*gy);
}

// Thin-line push: strengthen edges before upscaling (Anime4K Push mode)
std::vector<float> pushEdges(const std::vector<float>& src,
                               int w, int h, float strength = 0.5f) {
    std::vector<float> out(src.size());
    // Extract luma
    std::vector<float> luma(w * h);
    for (int i = 0; i < w * h; ++i)
        luma[i] = 0.2126f * src[i*3+0] + 0.7152f * src[i*3+1] + 0.0722f * src[i*3+2];

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float edge = sobelEdge(luma, w, h, x, y);
            float boost = 1.f + strength * std::min(edge * 4.f, 1.f);
            // Boost edge pixels while preserving flat areas
            for (int c = 0; c < 3; ++c) {
                float v = src[(y*w+x)*3+c];
                out[(y*w+x)*3+c] = std::clamp(v * boost, 0.f, 1.f);
            }
        }
    }
    return out;
}

// Adaptive sharpen post-pass: USM (Unsharp Mask) on upscaled output
std::vector<float> adaptiveSharpen(const std::vector<float>& src,
                                    int w, int h, float amount = 0.3f) {
    std::vector<float> blurred(src.size(), 0.f);
    // 3×3 box blur
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 3; ++c) {
                float sum = 0; int cnt = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int ny = std::clamp(y+dy, 0, h-1);
                        int nx = std::clamp(x+dx, 0, w-1);
                        sum += src[(ny*w+nx)*3+c];
                        ++cnt;
                    }
                }
                blurred[(y*w+x)*3+c] = sum / cnt;
            }
        }
    }
    // USM: out = src + amount * (src - blur)
    std::vector<float> out(src.size());
    for (size_t i = 0; i < src.size(); ++i)
        out[i] = std::clamp(src[i] + amount * (src[i] - blurred[i]), 0.f, 1.f);
    return out;
}

} // anon

Anime4K::Anime4K() = default;
Anime4K::~Anime4K() { shutdown(); }

bool Anime4K::init(const UpscalerConfig& cfg) {
    m_cfg         = cfg;
    m_initialized = true;
    return true;
}

void Anime4K::shutdown() {
    m_initialized = false;
}

video::VideoFramePtr Anime4K::process(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return frame;

    const int scale = static_cast<int>(m_cfg.factor);
    const int srcW  = frame->width();
    const int srcH  = frame->height();
    const int outW  = srcW * scale;
    const int outH  = srcH * scale;

    // Step 1: Extract RGB
    auto rgb = frameToRGB(*frame);

    // Step 2: Push edges before upscale (Anime4K characteristic pre-pass)
    float pushStrength = (m_cfg.denoiseStrength > 0.f) ? m_cfg.denoiseStrength : 0.4f;
    auto pushed = pushEdges(rgb, srcW, srcH, pushStrength);

    // Step 3: Bilinear × scale (GPU Vulkan shader replaces this at runtime)
    auto upscaled = bilinearResize(pushed, srcW, srcH, 3, outW, outH);

    // Step 4: Adaptive sharpen post-pass
    auto sharpened = adaptiveSharpen(upscaled, outW, outH, 0.25f);

    // Build output frame
    auto out = std::make_shared<video::VideoFrame>(outW, outH, video::PixelFormat::YUV420P);
    rgbToFrame(sharpened, outW, outH, *out);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    out->setColorMeta(frame->colorMeta());
    return out;
}

} // namespace aurora::upscaler

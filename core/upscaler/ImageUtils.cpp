// ─────────────────────────────────────────────────────────────────────────────
// ImageUtils.cpp  —  Aurora Motion Player
// ─────────────────────────────────────────────────────────────────────────────
// MSVC requires _USE_MATH_DEFINES before <cmath> to expose M_PI etc.
#if defined(_MSC_VER) || defined(_WIN32)
#  define _USE_MATH_DEFINES
#endif
#include "ImageUtils.h"
#include "video/VideoFrame.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aurora::upscaler {

// ── frameToRGB ────────────────────────────────────────────────────────────────
std::vector<float> frameToRGB(const video::VideoFrame& frame) {
    const int W = frame.width();
    const int H = frame.height();
    std::vector<float> rgb(W * H * 3, 0.f);

    switch (frame.format()) {
    case video::PixelFormat::RGB24: {
        const auto* src = frame.data(0);
        for (int i = 0; i < W * H; ++i) {
            rgb[i * 3 + 0] = src[i * 3 + 0] / 255.f;
            rgb[i * 3 + 1] = src[i * 3 + 1] / 255.f;
            rgb[i * 3 + 2] = src[i * 3 + 2] / 255.f;
        }
        break;
    }
    case video::PixelFormat::RGBA: {
        const auto* src = frame.data(0);
        for (int i = 0; i < W * H; ++i) {
            rgb[i * 3 + 0] = src[i * 4 + 0] / 255.f;
            rgb[i * 3 + 1] = src[i * 4 + 1] / 255.f;
            rgb[i * 3 + 2] = src[i * 4 + 2] / 255.f;
        }
        break;
    }
    case video::PixelFormat::BGRA: {
        const auto* src = frame.data(0);
        for (int i = 0; i < W * H; ++i) {
            rgb[i * 3 + 0] = src[i * 4 + 2] / 255.f;
            rgb[i * 3 + 1] = src[i * 4 + 1] / 255.f;
            rgb[i * 3 + 2] = src[i * 4 + 0] / 255.f;
        }
        break;
    }
    case video::PixelFormat::YUV420P: {
        // BT.709 limited range YCbCr → RGB
        const auto* Y  = frame.data(0);
        const auto* Cb = frame.data(1);
        const auto* Cr = frame.data(2);
        const int strideY  = frame.linesize(0);
        const int strideCb = frame.linesize(1);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float Yv  = (Y[y * strideY + x]              - 16.f)  / 219.f;
                float Cbv = (Cb[(y / 2) * strideCb + (x / 2)] - 128.f) / 224.f;
                float Crv = (Cr[(y / 2) * strideCb + (x / 2)] - 128.f) / 224.f;
                float R = std::clamp(Yv + 1.57480f * Crv, 0.f, 1.f);
                float G = std::clamp(Yv - 0.18733f * Cbv - 0.46813f * Crv, 0.f, 1.f);
                float B = std::clamp(Yv + 1.85563f * Cbv, 0.f, 1.f);
                int idx = (y * W + x) * 3;
                rgb[idx + 0] = R;
                rgb[idx + 1] = G;
                rgb[idx + 2] = B;
            }
        }
        break;
    }
    case video::PixelFormat::NV12: {
        const auto* Y  = frame.data(0);
        const auto* UV = frame.data(1);
        const int strideY  = frame.linesize(0);
        const int strideUV = frame.linesize(1);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float Yv  = (Y[y * strideY + x]                    - 16.f) / 219.f;
                float Cbv = (UV[(y / 2) * strideUV + (x & ~1)]     - 128.f) / 224.f;
                float Crv = (UV[(y / 2) * strideUV + (x & ~1) + 1] - 128.f) / 224.f;
                float R = std::clamp(Yv + 1.57480f * Crv, 0.f, 1.f);
                float G = std::clamp(Yv - 0.18733f * Cbv - 0.46813f * Crv, 0.f, 1.f);
                float B = std::clamp(Yv + 1.85563f * Cbv, 0.f, 1.f);
                int idx = (y * W + x) * 3;
                rgb[idx + 0] = R;
                rgb[idx + 1] = G;
                rgb[idx + 2] = B;
            }
        }
        break;
    }
    default:
        // Fallback: leave black
        break;
    }
    return rgb;
}

// ── rgbToFrame ────────────────────────────────────────────────────────────────
void rgbToFrame(const std::vector<float>& rgb,
                int width, int height,
                video::VideoFrame& out) {
    switch (out.format()) {
    case video::PixelFormat::RGB24: {
        auto* dst = out.data(0);
        for (int i = 0; i < width * height; ++i) {
            dst[i * 3 + 0] = static_cast<uint8_t>(std::clamp(rgb[i * 3 + 0], 0.f, 1.f) * 255.f + .5f);
            dst[i * 3 + 1] = static_cast<uint8_t>(std::clamp(rgb[i * 3 + 1], 0.f, 1.f) * 255.f + .5f);
            dst[i * 3 + 2] = static_cast<uint8_t>(std::clamp(rgb[i * 3 + 2], 0.f, 1.f) * 255.f + .5f);
        }
        break;
    }
    case video::PixelFormat::YUV420P: {
        // RGB → BT.709 limited range YCbCr
        auto* Y  = out.data(0);
        auto* Cb = out.data(1);
        auto* Cr = out.data(2);
        const int strideY  = out.linesize(0);
        const int strideCb = out.linesize(1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = (y * width + x) * 3;
                float R = rgb[idx + 0];
                float G = rgb[idx + 1];
                float B = rgb[idx + 2];
                float Yv  = 0.21260f * R + 0.71520f * G + 0.07220f * B;
                float Cbv = (B - Yv) / 1.85563f;
                float Crv = (R - Yv) / 1.57480f;
                Y[y * strideY + x] = static_cast<uint8_t>(
                    std::clamp(Yv * 219.f + 16.5f, 16.f, 235.f));
                if ((y & 1) == 0 && (x & 1) == 0) {
                    Cb[(y / 2) * strideCb + (x / 2)] = static_cast<uint8_t>(
                        std::clamp(Cbv * 224.f + 128.5f, 16.f, 240.f));
                    Cr[(y / 2) * strideCb + (x / 2)] = static_cast<uint8_t>(
                        std::clamp(Crv * 224.f + 128.5f, 16.f, 240.f));
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

// ── bilinearResize ────────────────────────────────────────────────────────────
std::vector<float> bilinearResize(const std::vector<float>& src,
                                   int srcW, int srcH, int channels,
                                   int dstW, int dstH) {
    std::vector<float> dst(dstW * dstH * channels, 0.f);
    float scaleX = static_cast<float>(srcW) / dstW;
    float scaleY = static_cast<float>(srcH) / dstH;
    for (int dy = 0; dy < dstH; ++dy) {
        float sy = (dy + .5f) * scaleY - .5f;
        int y0 = std::max(0, static_cast<int>(sy));
        int y1 = std::min(srcH - 1, y0 + 1);
        float fy = sy - y0;
        for (int dx = 0; dx < dstW; ++dx) {
            float sx = (dx + .5f) * scaleX - .5f;
            int x0 = std::max(0, static_cast<int>(sx));
            int x1 = std::min(srcW - 1, x0 + 1);
            float fx = sx - x0;
            for (int c = 0; c < channels; ++c) {
                float v00 = src[(y0 * srcW + x0) * channels + c];
                float v10 = src[(y0 * srcW + x1) * channels + c];
                float v01 = src[(y1 * srcW + x0) * channels + c];
                float v11 = src[(y1 * srcW + x1) * channels + c];
                float v = v00 * (1 - fx) * (1 - fy)
                        + v10 * fx       * (1 - fy)
                        + v01 * (1 - fx) * fy
                        + v11 * fx       * fy;
                dst[(dy * dstW + dx) * channels + c] = v;
            }
        }
    }
    return dst;
}

// ── padToAlign ────────────────────────────────────────────────────────────────
std::vector<float> padToAlign(const std::vector<float>& src,
                               int srcW, int srcH, int channels,
                               int align, int& padW, int& padH) {
    padW = ((srcW + align - 1) / align) * align;
    padH = ((srcH + align - 1) / align) * align;
    if (padW == srcW && padH == srcH) return src;
    std::vector<float> dst(padW * padH * channels, 0.f);
    for (int y = 0; y < srcH; ++y)
        for (int x = 0; x < srcW; ++x)
            for (int c = 0; c < channels; ++c)
                dst[(y * padW + x) * channels + c] = src[(y * srcW + x) * channels + c];
    return dst;
}

// ── cropBuffer ────────────────────────────────────────────────────────────────
std::vector<float> cropBuffer(const std::vector<float>& src,
                               int srcW, int srcH, int channels,
                               int cropW, int cropH) {
    (void)srcH;
    std::vector<float> dst(cropW * cropH * channels, 0.f);
    for (int y = 0; y < cropH; ++y)
        for (int x = 0; x < cropW; ++x)
            for (int c = 0; c < channels; ++c)
                dst[(y * cropW + x) * channels + c] = src[(y * srcW + x) * channels + c];
    return dst;
}

// ── normalize ─────────────────────────────────────────────────────────────────
void normalize(std::vector<float>& buf,
               float mean0, float mean1, float mean2,
               float std0, float std1, float std2) {
    const size_t N = buf.size() / 3;
    for (size_t i = 0; i < N; ++i) {
        buf[i * 3 + 0] = (buf[i * 3 + 0] - mean0) / std0;
        buf[i * 3 + 1] = (buf[i * 3 + 1] - mean1) / std1;
        buf[i * 3 + 2] = (buf[i * 3 + 2] - mean2) / std2;
    }
}

// ── floatToUint8 ──────────────────────────────────────────────────────────────
std::vector<uint8_t> floatToUint8(const std::vector<float>& buf,
                                   float lo, float hi) {
    std::vector<uint8_t> out(buf.size());
    float range = hi - lo;
    for (size_t i = 0; i < buf.size(); ++i)
        out[i] = static_cast<uint8_t>(std::clamp((buf[i] - lo) / range, 0.f, 1.f) * 255.f + .5f);
    return out;
}

// ── hwcToNchw ─────────────────────────────────────────────────────────────────
std::vector<float> hwcToNchw(const std::vector<float>& hwc, int width, int height) {
    std::vector<float> nchw(3 * height * width);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < height; ++h)
            for (int w = 0; w < width; ++w)
                nchw[c * height * width + h * width + w] = hwc[(h * width + w) * 3 + c];
    return nchw;
}

// ── nchwToHwc ─────────────────────────────────────────────────────────────────
std::vector<float> nchwToHwc(const std::vector<float>& nchw, int width, int height) {
    std::vector<float> hwc(height * width * 3);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < height; ++h)
            for (int w = 0; w < width; ++w)
                hwc[(h * width + w) * 3 + c] = nchw[c * height * width + h * width + w];
    return hwc;
}

// ── computeTiles ──────────────────────────────────────────────────────────────
std::vector<TileRect> computeTiles(int imgW, int imgH,
                                    int tileSize, int overlap, int align) {
    std::vector<TileRect> tiles;
    int step = tileSize - 2 * overlap;
    if (step <= 0) step = tileSize;
    for (int y = 0; y < imgH; y += step) {
        for (int x = 0; x < imgW; x += step) {
            TileRect t;
            t.x = x;  t.y = y;
            t.w = std::min(tileSize, imgW - x);
            t.h = std::min(tileSize, imgH - y);
            // Pad to alignment
            int pw = ((t.w + align - 1) / align) * align;
            int ph = ((t.h + align - 1) / align) * align;
            t.padRight  = pw - t.w;
            t.padBottom = ph - t.h;
            t.w = pw;  t.h = ph;
            tiles.push_back(t);
        }
    }
    return tiles;
}

// ── extractTile ───────────────────────────────────────────────────────────────
std::vector<float> extractTile(const std::vector<float>& src,
                                int srcW, int srcH, int channels,
                                const TileRect& tile) {
    std::vector<float> out(tile.w * tile.h * channels, 0.f);
    int srcH2 = srcH;  (void)srcH2;
    for (int y = 0; y < tile.h; ++y) {
        int sy = std::min(tile.y + y, srcH - 1);
        for (int x = 0; x < tile.w; ++x) {
            int sx = std::min(tile.x + x, srcW - 1);
            for (int c = 0; c < channels; ++c)
                out[(y * tile.w + x) * channels + c] = src[(sy * srcW + sx) * channels + c];
        }
    }
    return out;
}

// ── buildCosineWeights ────────────────────────────────────────────────────────
std::vector<float> buildCosineWeights(int w, int h, int overlap) {
    std::vector<float> wx(w, 1.f), wy(h, 1.f);
    for (int i = 0; i < overlap && i < w / 2; ++i) {
        float v = 0.5f * (1.f - std::cos(static_cast<float>(M_PI) * i / overlap));
        wx[i] = v;
        wx[w - 1 - i] = v;
    }
    for (int j = 0; j < overlap && j < h / 2; ++j) {
        float v = 0.5f * (1.f - std::cos(static_cast<float>(M_PI) * j / overlap));
        wy[j] = v;
        wy[h - 1 - j] = v;
    }
    std::vector<float> weights(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            weights[y * w + x] = wx[x] * wy[y];
    return weights;
}

// ── blendTile ─────────────────────────────────────────────────────────────────
void blendTile(const std::vector<float>& tile,
               const std::vector<float>& weights,
               int srcW, int srcH,
               int scaleFactor,
               int channels,
               const TileRect& tileRect,
               std::vector<float>& accum,
               std::vector<float>& weightSum) {
    int outW = srcW * scaleFactor;
    (void)srcH;
    int tX = tileRect.x * scaleFactor;
    int tY = tileRect.y * scaleFactor;
    int tW = tileRect.w;
    int tH = tileRect.h;
    for (int y = 0; y < tH; ++y) {
        int gy = tY + y;
        if (gy >= static_cast<int>(weightSum.size() / outW)) break;
        for (int x = 0; x < tW; ++x) {
            int gx = tX + x;
            if (gx >= outW) break;
            float w = weights[y * tW + x];
            weightSum[gy * outW + gx] += w;
            for (int c = 0; c < channels; ++c)
                accum[(gy * outW + gx) * channels + c] += tile[(y * tW + x) * channels + c] * w;
        }
    }
}

} // namespace aurora::upscaler

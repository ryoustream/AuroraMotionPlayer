#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ImageUtils.h  —  Aurora Motion Player
// Utility helpers for pixel-buffer ↔ VideoFrame conversions used by all
// upscaler and AI-inference modules.
// ─────────────────────────────────────────────────────────────────────────────
#include "video/VideoFrame.h"
#include <vector>
#include <cstdint>
#include <cassert>

namespace aurora::upscaler {

// ── RGB float buffer (HWC, normalized 0..1) ──────────────────────────────────

/// Extract a planar RGB float buffer (0..1) from a VideoFrame.
/// Supports YUV420P, NV12, RGBA, BGRA, RGB24 inputs.
/// Returns width * height * 3 floats (R,G,B interleaved).
std::vector<float> frameToRGB(const video::VideoFrame& frame);

/// Write a planar RGB float buffer (0..1) back into an existing VideoFrame.
/// The frame must already have the correct dimensions and pixel format.
void rgbToFrame(const std::vector<float>& rgb,
                int width, int height,
                video::VideoFrame& out);

// ── Bilinear resize (CPU fallback) ───────────────────────────────────────────

/// Bilinear-resize a CHW float buffer (C channels, H rows, W cols).
std::vector<float> bilinearResize(const std::vector<float>& src,
                                  int srcW, int srcH, int channels,
                                  int dstW, int dstH);

// ── Pad / Crop ────────────────────────────────────────────────────────────────

/// Pad src to the nearest multiple of `align` (zero-pad right/bottom).
/// Returns the padded buffer and sets padW / padH.
std::vector<float> padToAlign(const std::vector<float>& src,
                               int srcW, int srcH, int channels,
                               int align,
                               int& padW, int& padH);

/// Crop a center region out of a padded buffer.
std::vector<float> cropBuffer(const std::vector<float>& src,
                               int srcW, int srcH, int channels,
                               int cropW, int cropH);

// ── Normalize / De-normalize ──────────────────────────────────────────────────

/// Normalize a uint8 RGB frame into float [0,1] with optional mean/std.
void normalize(std::vector<float>& buf,
               float mean0 = 0.f, float mean1 = 0.f, float mean2 = 0.f,
               float std0  = 1.f, float std1  = 1.f, float std2  = 1.f);

/// Clamp float buffer to [lo, hi] and convert to uint8.
std::vector<uint8_t> floatToUint8(const std::vector<float>& buf,
                                   float lo = 0.f, float hi = 1.f);

// ── ONNX tensor helpers ───────────────────────────────────────────────────────

/// Convert HWC float RGB buffer → NCHW tensor (batch=1).
/// Returns shape {1, 3, H, W}.
std::vector<float> hwcToNchw(const std::vector<float>& hwc,
                               int width, int height);

/// Convert NCHW tensor (batch=1) → HWC float RGB buffer.
std::vector<float> nchwToHwc(const std::vector<float>& nchw,
                               int width, int height);

// ── Tile helpers ──────────────────────────────────────────────────────────────

struct TileRect {
    int x, y, w, h;       ///< Tile origin and size (pixels)
    int padRight, padBottom; ///< Padding applied to reach a power-of-2
};

/// Compute tile rectangles for an image of size (imgW x imgH) with the given
/// tile size and overlap.  Tiles are padded to a multiple of `align`.
std::vector<TileRect> computeTiles(int imgW, int imgH,
                                    int tileSize, int overlap,
                                    int align = 4);

/// Extract a single tile from a HWC buffer.
std::vector<float> extractTile(const std::vector<float>& src,
                                int srcW, int srcH, int channels,
                                const TileRect& tile);

/// Blend a processed tile back into a destination HWC buffer using a
/// cosine-window weight map (accumulate-then-divide approach).
void blendTile(const std::vector<float>& tile,
               const std::vector<float>& weights,
               int srcW, int srcH,           ///< original (un-scaled) dims
               int scaleFactor,
               int channels,
               const TileRect& tileRect,
               std::vector<float>& accum,    ///< accumulation buffer  (outW×outH×C)
               std::vector<float>& weightSum);///< weight-sum buffer   (outW×outH)

/// Build a cosine-window weight map for a tile of size (w × h).
std::vector<float> buildCosineWeights(int w, int h, int overlap);

} // namespace aurora::upscaler

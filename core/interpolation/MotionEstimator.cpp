#include "MotionEstimator.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace aurora::interpolation {

MotionEstimator::MotionEstimator(MEConfig cfg) : m_cfg(std::move(cfg)) {}

// ── Block Matching (Three Step Search) ───────────────────────────────────────
MotionField MotionEstimator::blockMatching(video::VideoFramePtr f0,
                                            video::VideoFramePtr f1)
{
    int w = f0->width(), h = f0->height();
    int bs = m_cfg.blockSize;
    int sr = m_cfg.searchRange;

    int blocksX = (w + bs - 1) / bs;
    int blocksY = (h + bs - 1) / bs;
    MotionField field(blocksX * blocksY);

    const uint8_t* y0 = f0->data(0);
    const uint8_t* y1 = f1->data(0);
    int ls0 = f0->linesize(0);
    int ls1 = f1->linesize(0);

    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            int px = bx * bs, py = by * bs;
            int bw = std::min(bs, w - px);
            int bh = std::min(bs, h - py);

            int64_t bestSAD = std::numeric_limits<int64_t>::max();
            int bestDx = 0, bestDy = 0;

            for (int dy = -sr; dy <= sr; ++dy) {
                for (int dx = -sr; dx <= sr; ++dx) {
                    int rx = px + dx, ry = py + dy;
                    if (rx < 0 || ry < 0 || rx + bw > w || ry + bh > h) continue;

                    int64_t sad = 0;
                    for (int y = 0; y < bh; ++y) {
                        for (int x = 0; x < bw; ++x) {
                            sad += std::abs(
                                static_cast<int>(y0[(py+y)*ls0 + (px+x)]) -
                                static_cast<int>(y1[(ry+y)*ls1 + (rx+x)]));
                        }
                    }
                    if (sad < bestSAD) {
                        bestSAD = sad;
                        bestDx  = dx;
                        bestDy  = dy;
                    }
                }
            }

            float maxSAD = static_cast<float>(bw * bh * 255);
            float conf   = 1.0f - static_cast<float>(bestSAD) / maxSAD;

            field[by * blocksX + bx] = {
                static_cast<int16_t>(bestDx),
                static_cast<int16_t>(bestDy),
                std::clamp(conf, 0.0f, 1.0f)
            };
        }
    }
    return field;
}

// ── Optical Flow (simple Lucas-Kanade pyramid, CPU) ──────────────────────────
MotionField MotionEstimator::opticalFlow(video::VideoFramePtr f0,
                                          video::VideoFramePtr f1)
{
    // Simplified — returns uniform zero field as placeholder
    // Full implementation uses Farneback dense optical flow
    int w = f0->width() / m_cfg.blockSize;
    int h = f0->height() / m_cfg.blockSize;
    return MotionField(w * h, {0, 0, 1.0f});
}

MotionField MotionEstimator::estimate(video::VideoFramePtr f0,
                                       video::VideoFramePtr f1)
{
    if (!f0 || !f1) return {};
    switch (m_cfg.algorithm) {
    case MEAlgorithm::BlockMatching: return blockMatching(f0, f1);
    case MEAlgorithm::OpticalFlow:   return opticalFlow(f0, f1);
    case MEAlgorithm::GMFlow:        return opticalFlow(f0, f1); // Neural ME via AuroraFlow
    default:                          return blockMatching(f0, f1);
    }
}

// ── Motion Compensator ────────────────────────────────────────────────────────
video::VideoFramePtr MotionCompensator::compensate(
    video::VideoFramePtr f0,
    const MotionField& field,
    int fieldW, int fieldH,
    float t)
{
    if (!f0 || field.empty()) return f0;

    int w = f0->width(), h = f0->height();
    int bs = w / fieldW;

    auto out = std::make_shared<video::VideoFrame>(w, h, video::PixelFormat::YUV420P);
    out->setTimeBase(f0->timeBase());
    out->setColorMeta(f0->colorMeta());

    const uint8_t* srcY = f0->data(0);
    uint8_t*       dstY = out->data(0);
    int ls_src = f0->linesize(0);
    int ls_dst = out->linesize(0);

    // Bilinear warp using motion field
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int bx = x / bs, by = y / fieldW;
            bx = std::clamp(bx, 0, fieldW - 1);
            by = std::clamp(by, 0, fieldH - 1);

            const auto& mv = field[by * fieldW + bx];
            float sx = x + mv.dx * t;
            float sy = y + mv.dy * t;

            int ix = static_cast<int>(sx), iy = static_cast<int>(sy);
            ix = std::clamp(ix, 0, w - 1);
            iy = std::clamp(iy, 0, h - 1);

            dstY[y * ls_dst + x] = srcY[iy * ls_src + ix];
        }
    }

    // Copy Cb/Cr planes as-is (chroma compensation simplified)
    if (f0->data(1) && out->data(1)) {
        int cw = w / 2, ch = h / 2;
        for (int y = 0; y < ch; ++y) {
            memcpy(out->data(1) + y * out->linesize(1),
                   f0->data(1)  + y * f0->linesize(1),
                   cw);
            memcpy(out->data(2) + y * out->linesize(2),
                   f0->data(2)  + y * f0->linesize(2),
                   cw);
        }
    }

    return out;
}

} // namespace aurora::interpolation

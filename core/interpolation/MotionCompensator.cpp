#include "MotionCompensator.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace aurora::interpolation {

static float sampleBilinear(const uint8_t* plane, int stride,
                              int w, int h, float x, float y) noexcept {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, w - 1);
    int y1 = std::min(y0 + 1, h - 1);
    x0 = std::clamp(x0, 0, w - 1);
    y0 = std::clamp(y0, 0, h - 1);

    float fx = x - static_cast<float>(x0);
    float fy = y - static_cast<float>(y0);

    float v00 = plane[y0 * stride + x0];
    float v10 = plane[y0 * stride + x1];
    float v01 = plane[y1 * stride + x0];
    float v11 = plane[y1 * stride + x1];

    return v00 * (1 - fx) * (1 - fy)
         + v10 * fx * (1 - fy)
         + v01 * (1 - fx) * fy
         + v11 * fx * fy;
}

video::VideoFramePtr MotionCompensator::compensate(
    const video::VideoFrame& f0,
    const video::VideoFrame& f1,
    const std::vector<MotionVector>& mvs,
    float t,
    int blockSize) const
{
    int w = f0.width();
    int h = f0.height();

    auto out = std::make_shared<video::VideoFrame>(w, h, f0.format());
    out->setPts(static_cast<int64_t>(
        f0.pts() + (f1.pts() - f0.pts()) * t));
    out->setTimeBase(f0.timeBase());
    out->setColorMeta(f0.colorMeta());

    // Only handle YUV420P for now
    int cols = (w + blockSize - 1) / blockSize;

    for (int plane = 0; plane < 3; ++plane) {
        const uint8_t* src0 = f0.data(plane);
        const int      ls0  = f0.linesize(plane);
        const uint8_t* src1 = f1.data(plane);
        const int      ls1  = f1.linesize(plane);
        uint8_t*       dst  = out->data(plane);
        const int      lsd  = out->linesize(plane);

        int pw = (plane == 0) ? w : w / 2;
        int ph = (plane == 0) ? h : h / 2;
        int bs = (plane == 0) ? blockSize : blockSize / 2;

        for (int by = 0; by * bs < ph; ++by) {
            for (int bx = 0; bx * bs < pw; ++bx) {
                // Get MV for this block (from luma grid)
                int mvIdx = by * cols + bx;
                float mvx = 0.0f, mvy = 0.0f;
                if (mvIdx < (int)mvs.size()) {
                    mvx = mvs[mvIdx].dx * (plane == 0 ? 1.0f : 0.5f) * t;
                    mvy = mvs[mvIdx].dy * (plane == 0 ? 1.0f : 0.5f) * t;
                }

                int x0b = bx * bs;
                int y0b = by * bs;

                for (int py = 0; py < bs && (y0b + py) < ph; ++py) {
                    for (int px = 0; px < bs && (x0b + px) < pw; ++px) {
                        float srcX = x0b + px + mvx;
                        float srcY = y0b + py + mvy;
                        float v0 = sampleBilinear(src0, ls0, pw, ph, srcX, srcY);

                        float srcX1 = x0b + px - mvs[std::min(mvIdx, (int)mvs.size()-1)].dx * (1.0f - t);
                        float srcY1 = y0b + py - mvs[std::min(mvIdx, (int)mvs.size()-1)].dy * (1.0f - t);
                        float v1 = sampleBilinear(src1, ls1, pw, ph, srcX1, srcY1);

                        float blended = v0 * (1.0f - t) + v1 * t;
                        dst[(y0b + py) * lsd + (x0b + px)] =
                            static_cast<uint8_t>(std::clamp(blended, 0.0f, 255.0f));
                    }
                }
            }
        }
    }
    return out;
}

video::VideoFramePtr MotionCompensator::compensateBilateral(
    const video::VideoFrame& f0,
    const video::VideoFrame& f1,
    const std::vector<MotionVector>& forwardMVs,
    const std::vector<MotionVector>& backwardMVs,
    float t,
    int blockSize) const
{
    // Forward warp from f0 + backward warp from f1, then blend
    auto fwdFrame  = compensate(f0, f1, forwardMVs,  t,         blockSize);
    auto bwdFrame  = compensate(f1, f0, backwardMVs, 1.0f - t,  blockSize);

    int w = f0.width(), h = f0.height();
    auto out = std::make_shared<video::VideoFrame>(w, h, f0.format());
    out->setPts(fwdFrame->pts());
    out->setTimeBase(f0.timeBase());
    out->setColorMeta(f0.colorMeta());

    for (int plane = 0; plane < 3; ++plane) {
        const uint8_t* a = fwdFrame->data(plane);
        const uint8_t* b = bwdFrame->data(plane);
        uint8_t*       d = out->data(plane);
        int ls = out->linesize(plane);
        int ph = (plane == 0) ? h : h / 2;
        int pw = (plane == 0) ? w : w / 2;

        for (int y = 0; y < ph; ++y)
            for (int x = 0; x < pw; ++x)
                d[y * ls + x] = static_cast<uint8_t>(
                    (static_cast<int>(a[y * fwdFrame->linesize(plane) + x]) +
                     static_cast<int>(b[y * bwdFrame->linesize(plane) + x])) / 2);
    }
    return out;
}

} // namespace aurora::interpolation

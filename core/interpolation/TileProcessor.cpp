#include "TileProcessor.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace aurora::interpolation {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TileProcessor::TileProcessor(const TileConfig& cfg) : m_cfg(cfg) {}

// Cosine-window weight map: smoothly tapers to 0 at edges
std::vector<float> TileProcessor::buildWeightMap(int W, int H, int ov) const {
    std::vector<float> w(W * H, 1.0f);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float wx = 1.f, wy = 1.f;
            if (ov > 0) {
                if (x < ov)   wx = 0.5f * (1.f - std::cos((float)x         / ov * (float)M_PI));
                if (x >= W-ov) wx = 0.5f * (1.f - std::cos((float)(W-1-x)  / ov * (float)M_PI));
                if (y < ov)   wy = 0.5f * (1.f - std::cos((float)y         / ov * (float)M_PI));
                if (y >= H-ov) wy = 0.5f * (1.f - std::cos((float)(H-1-y)  / ov * (float)M_PI));
            }
            w[y*W+x] = wx * wy;
        }
    }
    return w;
}

std::vector<float> TileProcessor::process(
    const std::vector<float>& rgb0,
    const std::vector<float>& rgb1,
    int FW, int FH, float t,
    TileInferFn inferFn) const
{
    int ts  = m_cfg.tileSize;
    int ov  = m_cfg.overlap;
    int step = ts - ov * 2;
    if (step <= 0) throw std::runtime_error("TileProcessor: overlap >= tileSize/2");

    std::vector<float> output(FW * FH * 3, 0.f);
    std::vector<float> weight(FW * FH,     0.f);

    // Iterate tiles with overlap stride
    for (int ty = 0; ty < FH; ty += step) {
        for (int tx = 0; tx < FW; tx += step) {
            // Compute actual tile bounds (clamped to frame)
            int x0 = std::max(0, tx - ov);
            int y0 = std::max(0, ty - ov);
            int x1 = std::min(FW, tx + ts - ov);
            int y1 = std::min(FH, ty + ts - ov);
            int tW = x1 - x0, tH = y1 - y0;

            // Pad to tileSize if requested (models may require fixed size)
            int inferW = m_cfg.padToTile ? ts : tW;
            int inferH = m_cfg.padToTile ? ts : tH;

            // Extract tile from both frames
            auto extractTile = [&](const std::vector<float>& src) {
                std::vector<float> tile(inferW * inferH * 3, 0.f);
                for (int y = 0; y < tH; ++y)
                    for (int x = 0; x < tW; ++x) {
                        int si = ((y0+y)*FW + (x0+x))*3;
                        int di = (y*inferW + x)*3;
                        tile[di]=src[si]; tile[di+1]=src[si+1]; tile[di+2]=src[si+2];
                    }
                return tile;
            };

            auto t0 = extractTile(rgb0);
            auto t1 = extractTile(rgb1);

            // Run inference on this tile
            auto result = inferFn(t0, t1, inferW, inferH, t);
            if (result.empty()) continue;

            // Build weight map for this tile
            auto wmap = buildWeightMap(tW, tH, std::min(ov, std::min(tW,tH)/4));

            // Accumulate into output with weighted blending
            for (int y = 0; y < tH; ++y)
                for (int x = 0; x < tW; ++x) {
                    float wv = wmap[y*tW+x];
                    int oi = ((y0+y)*FW + (x0+x))*3;
                    int ri = (y*inferW + x)*3;
                    output[oi]   += result[ri]   * wv;
                    output[oi+1] += result[ri+1] * wv;
                    output[oi+2] += result[ri+2] * wv;
                    weight[(y0+y)*FW + (x0+x)] += wv;
                }
        }
    }

    // Normalize by accumulated weight
    for (int i = 0; i < FW*FH; ++i) {
        if (weight[i] > 1e-6f) {
            float inv = 1.f / weight[i];
            output[i*3]   *= inv;
            output[i*3+1] *= inv;
            output[i*3+2] *= inv;
        }
    }

    return output;
}

} // namespace aurora::interpolation

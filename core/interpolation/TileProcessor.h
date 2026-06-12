#pragma once
#include "video/VideoFrame.h"
#include <functional>
#include <vector>

namespace aurora::interpolation {

// TileProcessor splits large frames into overlapping tiles, runs inference
// per-tile, then blends seams with a cosine-window feather.
// This allows AI interpolation on GPUs with limited VRAM (< 4 GB).
struct TileConfig {
    int  tileSize    = 256;   // Tile width/height (pixels)
    int  overlap     = 32;    // Overlap on each edge for seamless blending
    bool padToTile   = true;  // Pad last tile to full size
};

using TileInferFn = std::function<
    std::vector<float>(const std::vector<float>& rgb0,
                       const std::vector<float>& rgb1,
                       int tileW, int tileH, float t)>;

class TileProcessor {
public:
    explicit TileProcessor(const TileConfig& cfg = {});

    // Process two full-frame RGB buffers tile by tile.
    // inferFn receives (rgb0_tile, rgb1_tile, tileW, tileH, t) → rgb_out_tile.
    std::vector<float> process(const std::vector<float>& rgb0,
                               const std::vector<float>& rgb1,
                               int frameW, int frameH,
                               float t,
                               TileInferFn inferFn) const;

    void setConfig(const TileConfig& cfg) { m_cfg = cfg; }
    const TileConfig& config() const { return m_cfg; }

private:
    // Build cosine-window blend weight map for a tile
    std::vector<float> buildWeightMap(int tileW, int tileH, int overlap) const;

    TileConfig m_cfg;
};

} // namespace aurora::interpolation

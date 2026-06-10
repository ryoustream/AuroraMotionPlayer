#pragma once
#include "SubtitleEngine.h"
#include <vector>
#include <cstdint>

namespace aurora::subtitle {

struct RenderedSubtitle {
    std::vector<uint8_t> rgba;   // RGBA pixels
    int x = 0, y = 0;
    int width = 0, height = 0;
    double startTime = 0.0;
    double endTime   = 0.0;
};

// Software subtitle renderer (CPU-based, for overlaying on video)
// GPU rendering is done in the Vulkan/OpenGL shader pipeline
class SubtitleRenderer {
public:
    SubtitleRenderer() = default;

    // Render all active subtitles at timestamp into RGBA bitmaps
    std::vector<RenderedSubtitle> render(
        const SubtitleEngine& engine,
        double timestamp,
        int videoWidth,
        int videoHeight);

    void setScale(float scale) noexcept { m_scale = scale; }

private:
    RenderedSubtitle renderEvent(const SubtitleEvent& ev,
                                  int videoWidth, int videoHeight);

    float m_scale = 1.0f;
};

} // namespace aurora::subtitle

#include "SubtitleRenderer.h"
#include <cstring>
#include <algorithm>

namespace aurora::subtitle {

std::vector<RenderedSubtitle> SubtitleRenderer::render(
    const SubtitleEngine& engine,
    double timestamp,
    int videoWidth,
    int videoHeight)
{
    std::vector<RenderedSubtitle> results;
    auto active = engine.getActiveAt(timestamp);
    for (const auto* ev : active) {
        if (!ev) continue;
        // For bitmap subtitles (PGS), return as-is
        if (!ev->bitmap.empty()) {
            RenderedSubtitle rs;
            rs.rgba      = ev->bitmap;
            rs.x         = ev->bitmapX;
            rs.y         = ev->bitmapY;
            rs.width     = ev->bitmapW;
            rs.height    = ev->bitmapH;
            rs.startTime = ev->startTime;
            rs.endTime   = ev->endTime;
            results.push_back(std::move(rs));
        } else {
            results.push_back(renderEvent(*ev, videoWidth, videoHeight));
        }
    }
    return results;
}

RenderedSubtitle SubtitleRenderer::renderEvent(
    const SubtitleEvent& ev,
    int videoWidth,
    int videoHeight)
{
    RenderedSubtitle rs;
    rs.startTime = ev.startTime;
    rs.endTime   = ev.endTime;

    // Simple software text rendering: create a small RGBA bitmap
    // In production this uses FreeType2 for proper font rendering
    // Here we return a minimal placeholder
    int fontSize   = static_cast<int>(ev.style.fontSize * m_scale);
    int textW      = static_cast<int>(ev.text.size() * fontSize * 0.6f);
    int textH      = fontSize + 4;
    int padding    = 8;

    rs.width  = textW + padding * 2;
    rs.height = textH + padding * 2;
    rs.x      = (videoWidth - rs.width) / 2;
    rs.y      = videoHeight - rs.height - ev.style.marginV;

    rs.rgba.assign(rs.width * rs.height * 4, 0);

    // Fill background (semi-transparent black) and white text placeholder
    for (int py = 0; py < rs.height; ++py) {
        for (int px = 0; px < rs.width; ++px) {
            int idx = (py * rs.width + px) * 4;
            // Semi-transparent black background
            rs.rgba[idx + 0] = 0;
            rs.rgba[idx + 1] = 0;
            rs.rgba[idx + 2] = 0;
            rs.rgba[idx + 3] = 160;
        }
    }

    return rs;
}

} // namespace aurora::subtitle

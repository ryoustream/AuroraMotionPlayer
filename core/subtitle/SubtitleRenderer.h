#pragma once
#include "SubtitleEngine.h"
#include "FreeTypeRenderer.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace aurora::subtitle {

struct RenderedSubtitle {
    std::vector<uint8_t> rgba;   // RGBA pixels
    int x = 0, y = 0;
    int width = 0, height = 0;
    double startTime = 0.0;
    double endTime   = 0.0;
};

/**
 * Software subtitle renderer (CPU-based, overlaid on the video frame
 * by the GPU compositor before presentation).
 *
 * Session 8 update:
 *  - Delegates text rendering to FreeTypeRenderer (real glyph rasterisation,
 *    outline + shadow, multi-line layout)
 *  - PGS/bitmap events are passed through as decoded RGBA directly
 *  - ASS-styled text uses the SubtitleEvent's per-event SubtitleStyle
 *    (font, colors, outline, shadow, alignment, margins)
 */
class SubtitleRenderer {
public:
    SubtitleRenderer();
    ~SubtitleRenderer();

    // Render all active subtitles at timestamp into RGBA bitmaps
    std::vector<RenderedSubtitle> render(
        const SubtitleEngine& engine,
        double timestamp,
        int videoWidth,
        int videoHeight);

    void setScale(float scale) noexcept { m_scale = scale; }
    float scale() const noexcept { return m_scale; }

    /** Provide an explicit font file to use (overrides system font lookup). */
    bool setFontFile(const std::string& path);

private:
    RenderedSubtitle renderEvent(const SubtitleEvent& ev,
                                  int videoWidth, int videoHeight);

    void ensureFontLoaded(const std::string& fontName, bool bold, bool italic);

    float m_scale = 1.0f;
    std::unique_ptr<FreeTypeRenderer> m_ftRenderer;
    std::string m_lastFontName;
    bool        m_lastBold   = false;
    bool        m_lastItalic = false;
    bool        m_fontReady  = false;
};

} // namespace aurora::subtitle

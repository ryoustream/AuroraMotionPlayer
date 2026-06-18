#include "SubtitleRenderer.h"
#include <cstring>
#include <algorithm>

namespace aurora::subtitle {

SubtitleRenderer::SubtitleRenderer()
    : m_ftRenderer(std::make_unique<FreeTypeRenderer>())
{
}

SubtitleRenderer::~SubtitleRenderer() = default;

bool SubtitleRenderer::setFontFile(const std::string& path)
{
    bool ok = m_ftRenderer->loadFont(path);
    m_fontReady = ok;
    return ok;
}

void SubtitleRenderer::ensureFontLoaded(
    const std::string& fontName, bool bold, bool italic)
{
    if (m_fontReady && fontName == m_lastFontName &&
        bold == m_lastBold && italic == m_lastItalic) {
        return; // already loaded
    }

    bool ok = m_ftRenderer->loadSystemFont(fontName, bold, italic);
    if (!ok) ok = m_ftRenderer->loadSystemFont("DejaVuSans", bold, italic);
    if (!ok) ok = m_ftRenderer->loadSystemFont("Roboto", bold, italic); // Android fallback
    if (!ok) ok = m_ftRenderer->loadSystemFont("Arial", bold, italic);

    m_fontReady   = ok;
    m_lastFontName= fontName;
    m_lastBold    = bold;
    m_lastItalic  = italic;
}

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

        // Bitmap subtitle (PGS/SUP): already decoded RGBA, pass through.
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
            continue;
        }

        // Text subtitle (SRT/ASS/VTT): rasterise via FreeType.
        if (!ev->text.empty()) {
            RenderedSubtitle rs = renderEvent(*ev, videoWidth, videoHeight);
            if (!rs.rgba.empty())
                results.push_back(std::move(rs));
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

    ensureFontLoaded(ev.style.fontName, ev.style.bold, ev.style.italic);

    if (m_fontReady) {
        FreeTypeRenderer::RenderParams rp;
        rp.text          = ev.text;
        rp.fontSize       = static_cast<int>(ev.style.fontSize * m_scale);
        rp.primaryColor   = (ev.style.primaryColor << 8) | 0xFF; // RGB→RGBA, opaque
        rp.outlineColor   = (ev.style.outlineColor << 8) | 0xFF;
        rp.shadowColor    = ev.style.shadowColor;
        rp.outlineWidth   = ev.style.outlineWidth * m_scale;
        rp.shadowOffX     = ev.style.shadowDepth * m_scale;
        rp.shadowOffY     = ev.style.shadowDepth * m_scale;
        rp.bold           = ev.style.bold;
        rp.italic         = ev.style.italic;
        rp.maxWidthPx     = videoWidth - ev.style.marginH * 2;
        rp.alignment      = ev.style.alignment;

        auto result = m_ftRenderer->render(rp);

        if (!result.rgba.empty()) {
            rs.rgba   = std::move(result.rgba);
            rs.width  = result.width;
            rs.height = result.height;

            // Position based on ASS-style numpad alignment (default: bottom-center)
            int col = (ev.style.alignment - 1) % 3;
            int row = (ev.style.alignment - 1) / 3;

            switch (col) {
                case 0: rs.x = ev.style.marginH; break;
                case 1: rs.x = (videoWidth - rs.width) / 2; break;
                case 2: rs.x = videoWidth - ev.style.marginH - rs.width; break;
                default: rs.x = (videoWidth - rs.width) / 2; break;
            }
            switch (row) {
                case 0: rs.y = videoHeight - ev.style.marginV - rs.height; break; // bottom
                case 1: rs.y = (videoHeight - rs.height) / 2; break;               // middle
                case 2: rs.y = ev.style.marginV; break;                            // top
                default: rs.y = videoHeight - ev.style.marginV - rs.height; break;
            }
            return rs;
        }
    }

    // ── Fallback: FreeType unavailable or render failed ────────────────────
    // Produce a semi-transparent placeholder box so playback doesn't
    // silently lose subtitle timing/positioning information.
    int fontSize = static_cast<int>(ev.style.fontSize * m_scale);
    int textW    = static_cast<int>(ev.text.size() * fontSize * 0.6f);
    int textH    = fontSize + 4;
    int padding  = 8;

    rs.width  = textW + padding * 2;
    rs.height = textH + padding * 2;
    rs.x      = (videoWidth - rs.width) / 2;
    rs.y      = videoHeight - rs.height - ev.style.marginV;

    rs.rgba.assign(static_cast<size_t>(rs.width * rs.height * 4), 0);
    for (size_t i = 0; i < rs.rgba.size(); i += 4) {
        rs.rgba[i + 3] = 160; // semi-transparent black background
    }
    return rs;
}

} // namespace aurora::subtitle

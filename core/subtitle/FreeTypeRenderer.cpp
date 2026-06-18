#include "FreeTypeRenderer.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>

// FreeType is optional at compile time. If not present, all render calls
// return empty results (graceful degradation).
#ifdef AURORA_HAS_FREETYPE
#  include <ft2build.h>
#  include FT_FREETYPE_H
#  include FT_STROKER_H
#  include FT_GLYPH_H
#  include FT_SYNTHESIS_H
#  define FT_AVAILABLE 1
#else
#  define FT_AVAILABLE 0
#  define FT_Library void*
#  define FT_Face    void*
#endif

namespace aurora::subtitle {

// ── Construction ──────────────────────────────────────────────────────────────

FreeTypeRenderer::FreeTypeRenderer()
{
#if FT_AVAILABLE
    FT_Error err = FT_Init_FreeType(&m_library);
    if (err) m_library = nullptr;
#endif
}

FreeTypeRenderer::~FreeTypeRenderer()
{
#if FT_AVAILABLE
    clearCache();
    if (m_face)    FT_Done_Face(m_face);
    if (m_library) FT_Done_FreeType(m_library);
#endif
}

// ── Font loading ──────────────────────────────────────────────────────────────

bool FreeTypeRenderer::loadFont(const std::string& path, int faceIndex)
{
#if FT_AVAILABLE
    if (!m_library) return false;
    if (m_face) { FT_Done_Face(m_face); m_face = nullptr; }
    FT_Error err = FT_New_Face(m_library, path.c_str(), faceIndex, &m_face);
    if (err) { m_face = nullptr; return false; }
    clearCache();
    return true;
#else
    (void)path; (void)faceIndex;
    return false;
#endif
}

bool FreeTypeRenderer::loadSystemFont(const std::string& family, bool bold, bool italic)
{
    // Common system font paths (Windows + Linux + Android)
    static const char* kSearchDirs[] = {
        "/system/fonts",          // Android
        "/usr/share/fonts",       // Linux
        "C:/Windows/Fonts",       // Windows
        "/usr/local/share/fonts",
        nullptr
    };

    std::string fname = family;
    if (bold && italic) fname += "-BoldItalic";
    else if (bold)      fname += "-Bold";
    else if (italic)    fname += "-Italic";

    static const char* kExts[] = { ".ttf", ".otf", ".ttc", nullptr };

    for (int d = 0; kSearchDirs[d]; ++d) {
        for (int e = 0; kExts[e]; ++e) {
            std::string path = std::string(kSearchDirs[d]) + "/" + fname + kExts[e];
            if (loadFont(path)) return true;
            // Also try lowercase
            std::string lower = family;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            path = std::string(kSearchDirs[d]) + "/" + lower + kExts[e];
            if (loadFont(path)) return true;
        }
    }
    return false;
}

// ── UTF-8 decoder ─────────────────────────────────────────────────────────────

uint32_t FreeTypeRenderer::nextCodepoint(const char*& p, const char* end) noexcept
{
    if (p >= end) return 0;
    auto u = [](char c) -> uint32_t { return static_cast<unsigned char>(c); };
    uint32_t cp;
    if ((u(*p) & 0x80) == 0) {
        cp = u(*p++);
    } else if ((u(*p) & 0xE0) == 0xC0 && p + 1 < end) {
        cp = (u(p[0]) & 0x1F) << 6 | (u(p[1]) & 0x3F);
        p += 2;
    } else if ((u(*p) & 0xF0) == 0xE0 && p + 2 < end) {
        cp = (u(p[0]) & 0x0F) << 12 | (u(p[1]) & 0x3F) << 6 | (u(p[2]) & 0x3F);
        p += 3;
    } else if ((u(*p) & 0xF8) == 0xF0 && p + 3 < end) {
        cp = (u(p[0]) & 0x07) << 18 | (u(p[1]) & 0x3F) << 12 |
             (u(p[2]) & 0x3F) << 6  | (u(p[3]) & 0x3F);
        p += 4;
    } else {
        cp = u(*p++);
    }
    return cp;
}

// ── Glyph cache ───────────────────────────────────────────────────────────────

const FreeTypeRenderer::GlyphBitmap* FreeTypeRenderer::getGlyph(
    uint32_t cp, int size, bool bold, bool italic, float outlineW)
{
    CacheKey key{cp, size, bold, italic};
    auto it = m_glyphCache.find(key);
    if (it != m_glyphCache.end()) return &it->second;

    if (m_glyphCache.size() >= kMaxCacheEntries) clearCache();

#if FT_AVAILABLE
    if (!m_face || !m_library) return nullptr;

    FT_Set_Pixel_Sizes(m_face, 0, static_cast<FT_UInt>(size));

    // Synthesise bold / italic if no dedicated face
    FT_GlyphSlot slot = m_face->glyph;
    if (bold)   FT_GlyphSlot_Embolden(slot);
    if (italic) FT_GlyphSlot_Oblique(slot);

    FT_UInt glyphIdx = FT_Get_Char_Index(m_face, cp);
    if (FT_Load_Glyph(m_face, glyphIdx, FT_LOAD_DEFAULT)) return nullptr;
    if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL))     return nullptr;

    FT_Bitmap& bm = slot->bitmap;
    GlyphBitmap gb;
    gb.width    = static_cast<int>(bm.width);
    gb.height   = static_cast<int>(bm.rows);
    gb.bearingX = slot->bitmap_left;
    gb.bearingY = slot->bitmap_top;
    gb.advance  = static_cast<int>(slot->advance.x);

    // Copy alpha channel
    gb.alpha.assign(bm.buffer, bm.buffer + gb.width * gb.height);

    // Generate outline via FT_Stroker
    if (outlineW > 0.0f) {
        FT_Stroker stroker;
        FT_Stroker_New(m_library, &stroker);
        FT_Stroker_Set(stroker,
            static_cast<FT_Fixed>(outlineW * 64),
            FT_STROKER_LINECAP_ROUND,
            FT_STROKER_LINEJOIN_ROUND,
            0);

        FT_Glyph glyph;
        if (FT_Get_Glyph(m_face->glyph, &glyph) == 0) {
            FT_Glyph_StrokeBorder(&glyph, stroker, false, true);
            FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
            FT_BitmapGlyph bmpGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
            FT_Bitmap& obm = bmpGlyph->bitmap;
            gb.outline.assign(obm.buffer,
                               obm.buffer + obm.width * obm.rows);
            FT_Done_Glyph(glyph);
        }
        FT_Stroker_Done(stroker);
    }

    auto [ins, ok] = m_glyphCache.emplace(key, std::move(gb));
    return ok ? &ins->second : nullptr;
#else
    (void)cp; (void)size; (void)bold; (void)italic; (void)outlineW;
    return nullptr;
#endif
}

void FreeTypeRenderer::clearCache() noexcept { m_glyphCache.clear(); }

// ── Layout ────────────────────────────────────────────────────────────────────

std::vector<FreeTypeRenderer::LineLayout> FreeTypeRenderer::layoutText(
    const std::string& utf8, int fontSize,
    bool bold, bool italic, float outlineW, int maxW)
{
    std::vector<LineLayout> lines;
    LineLayout current;

    auto flush = [&]() {
        if (!current.codepoints.empty()) {
            lines.push_back(std::move(current));
            current = LineLayout{};
        }
    };

    const char* p   = utf8.c_str();
    const char* end = p + utf8.size();

    while (p < end) {
        uint32_t cp = nextCodepoint(p, end);
        if (cp == '\n') { flush(); continue; }

        const GlyphBitmap* g = getGlyph(cp, fontSize, bold, italic, outlineW);
        if (!g) continue;

        int adv = (g->advance >> 6);
        if (current.totalAdvance + adv > maxW && !current.codepoints.empty()) {
            flush();
        }

        current.codepoints.push_back(cp);
        current.glyphs.push_back(g);
        current.totalAdvance += adv;
        int asc  = g->bearingY;
        int desc = g->height - g->bearingY;
        if (asc  > current.maxAscent)  current.maxAscent  = asc;
        if (desc > current.maxDescent) current.maxDescent = desc;
    }
    flush();
    return lines;
}

// ── Pixel operations ──────────────────────────────────────────────────────────

uint8_t FreeTypeRenderer::alphaBlend(
    uint8_t dst, uint8_t src, uint8_t alpha) noexcept
{
    return static_cast<uint8_t>(
        (static_cast<int>(src) * alpha +
         static_cast<int>(dst) * (255 - alpha)) / 255);
}

void FreeTypeRenderer::blendGlyph(
    std::vector<uint8_t>& canvas, int cW, int cH,
    const GlyphBitmap& glyph,
    int x, int y,
    uint32_t color, bool useOutline) noexcept
{
    const auto& bitmap = useOutline ? glyph.outline : glyph.alpha;
    int gW = glyph.width;
    int gH = glyph.height;

    uint8_t cr = (color >> 24) & 0xFF;
    uint8_t cg = (color >> 16) & 0xFF;
    uint8_t cb = (color >>  8) & 0xFF;
    uint8_t ca = (color      ) & 0xFF;

    for (int row = 0; row < gH; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= cH) continue;
        for (int col = 0; col < gW; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= cW) continue;
            size_t gi = static_cast<size_t>(row * gW + col);
            if (gi >= bitmap.size()) continue;
            uint8_t ga = static_cast<uint8_t>(
                static_cast<int>(bitmap[gi]) * ca / 255);
            if (ga == 0) continue;
            size_t ci = static_cast<size_t>((dy * cW + dx) * 4);
            canvas[ci]   = alphaBlend(canvas[ci],   cr, ga);
            canvas[ci+1] = alphaBlend(canvas[ci+1], cg, ga);
            canvas[ci+2] = alphaBlend(canvas[ci+2], cb, ga);
            canvas[ci+3] = std::max(canvas[ci+3], ga);
        }
    }
}

// ── Main render ───────────────────────────────────────────────────────────────

FreeTypeRenderer::RenderResult FreeTypeRenderer::render(const RenderParams& rp)
{
    RenderResult result;
    if (!m_face) return result;

    auto lines = layoutText(rp.text, rp.fontSize,
                             rp.bold, rp.italic,
                             rp.outlineWidth, rp.maxWidthPx);
    if (lines.empty()) return result;

    static const int kLineSpacing = 4;
    int totalH = 0;
    int maxW   = 0;
    for (auto& ln : lines) {
        totalH += ln.maxAscent + ln.maxDescent + kLineSpacing;
        maxW    = std::max(maxW, ln.totalAdvance);
    }
    totalH += static_cast<int>(rp.shadowOffY) + 2;
    maxW   += static_cast<int>(rp.shadowOffX) + 2;
    maxW   += static_cast<int>(rp.outlineWidth * 2) + 2;

    result.width  = maxW;
    result.height = totalH;
    result.rgba.assign(static_cast<size_t>(maxW * totalH * 4), 0);

    int padX = static_cast<int>(rp.outlineWidth) + 1;
    int penY = 0;

    for (auto& ln : lines) {
        penY += ln.maxAscent;
        int penX = padX;

        // Pass 1: Shadow
        if ((rp.shadowColor & 0xFF) > 0) {
            int sx = penX + static_cast<int>(rp.shadowOffX);
            int spy = penY + static_cast<int>(rp.shadowOffY);
            int px = sx;
            for (size_t gi = 0; gi < ln.glyphs.size(); ++gi) {
                const auto* g = ln.glyphs[gi];
                blendGlyph(result.rgba, maxW, totalH, *g,
                            px + g->bearingX, spy - g->bearingY,
                            rp.shadowColor, false);
                px += g->advance >> 6;
            }
        }

        // Pass 2: Outline
        if (rp.outlineWidth > 0 && (rp.outlineColor & 0xFF) > 0) {
            int px = penX;
            for (size_t gi = 0; gi < ln.glyphs.size(); ++gi) {
                const auto* g = ln.glyphs[gi];
                if (!g->outline.empty()) {
                    blendGlyph(result.rgba, maxW, totalH, *g,
                                px + g->bearingX - static_cast<int>(rp.outlineWidth),
                                penY - g->bearingY - static_cast<int>(rp.outlineWidth),
                                rp.outlineColor, true);
                }
                px += g->advance >> 6;
            }
        }

        // Pass 3: Fill
        {
            int px = penX;
            for (size_t gi = 0; gi < ln.glyphs.size(); ++gi) {
                const auto* g = ln.glyphs[gi];
                blendGlyph(result.rgba, maxW, totalH, *g,
                            px + g->bearingX, penY - g->bearingY,
                            rp.primaryColor, false);
                px += g->advance >> 6;
            }
        }

        penY += ln.maxDescent + kLineSpacing;
    }
    result.baselineY = lines.empty() ? 0 : lines[0].maxAscent;
    return result;
}

} // namespace aurora::subtitle

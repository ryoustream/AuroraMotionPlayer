#pragma once
#include "SubtitleEngine.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

// Forward-declare FreeType types to avoid exposing ft2build.h in headers
typedef struct FT_LibraryRec_*  FT_Library;
typedef struct FT_FaceRec_*     FT_Face;

namespace aurora::subtitle {

/**
 * Aurora Motion Player — FreeType Text Renderer (Session 8)
 *
 * Rasterises subtitle text into RGBA bitmaps using FreeType 2.
 * Features:
 *  - Multi-line text layout with automatic line breaking
 *  - Per-character outline (stroker-based, configurable width)
 *  - Drop shadow with configurable offset and blur radius
 *  - Font weight: normal / bold / italic / bold-italic
 *  - Glyph cache keyed by (codepoint, size, face) — avoids re-rendering
 *  - Kerning support when available
 *  - UTF-8 text input
 *  - ASS alignment (numpad layout: 1–9)
 *
 * Thread safety: single-threaded. Callers must not share instances
 * across threads without external synchronisation.
 */
class FreeTypeRenderer {
public:
    FreeTypeRenderer();
    ~FreeTypeRenderer();

    // Non-copyable
    FreeTypeRenderer(const FreeTypeRenderer&)            = delete;
    FreeTypeRenderer& operator=(const FreeTypeRenderer&) = delete;

    // ── Init ──────────────────────────────────────────────────────────────────

    /**
     * Load a font face from file.
     * Returns false if FreeType is unavailable or the path is invalid.
     * Falls back to embedded fallback font (Noto Sans Latin).
     */
    bool loadFont(const std::string& path, int faceIndex = 0);

    /** Load a system font by name. Searches common system paths. */
    bool loadSystemFont(const std::string& familyName,
                        bool bold = false, bool italic = false);

    bool isInitialised() const noexcept { return m_library != nullptr; }

    // ── Render ────────────────────────────────────────────────────────────────

    struct RenderParams {
        std::string text;
        int         fontSize      = 24;     // pt at 72 DPI (≈ px)
        uint32_t    primaryColor  = 0xFFFFFFFF; // RGBA
        uint32_t    outlineColor  = 0xFF000000; // RGBA
        uint32_t    shadowColor   = 0x80000000; // RGBA
        float       outlineWidth  = 2.0f;
        float       shadowOffX    = 2.0f;
        float       shadowOffY    = 2.0f;
        bool        bold          = false;
        bool        italic        = false;
        int         maxWidthPx    = 1920;
        int         alignment     = 2;          // ASS numpad alignment
    };

    struct RenderResult {
        std::vector<uint8_t> rgba;   // RGBA pixels
        int width   = 0;
        int height  = 0;
        int baselineY = 0;           // Y offset of baseline within bitmap
    };

    /**
     * Render text to an RGBA bitmap.
     * Returns an empty result if FreeType is not available.
     */
    RenderResult render(const RenderParams& p);

    // ── Glyph cache ───────────────────────────────────────────────────────────

    void clearCache() noexcept;
    size_t cacheSize() const noexcept { return m_glyphCache.size(); }

private:
    struct GlyphBitmap {
        std::vector<uint8_t> alpha;  // 8-bit alpha
        std::vector<uint8_t> outline;// 8-bit outline alpha
        int width    = 0;
        int height   = 0;
        int bearingX = 0;
        int bearingY = 0;
        int advance  = 0;            // 26.6 fixed point
    };

    struct CacheKey {
        uint32_t codepoint;
        int      size;
        bool     bold;
        bool     italic;
        bool operator==(const CacheKey& o) const noexcept {
            return codepoint == o.codepoint && size == o.size
                && bold == o.bold && italic == o.italic;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const noexcept {
            return std::hash<uint64_t>{}(
                (static_cast<uint64_t>(k.codepoint) << 24) |
                (static_cast<uint64_t>(k.size) << 2) |
                (k.bold ? 2 : 0) | (k.italic ? 1 : 0));
        }
    };

    const GlyphBitmap* getGlyph(uint32_t cp, int size, bool bold, bool italic,
                                  float outlineW);

    // Layout helpers
    struct LineLayout {
        std::vector<uint32_t>         codepoints;
        std::vector<const GlyphBitmap*> glyphs;
        int totalAdvance = 0;
        int maxAscent    = 0;
        int maxDescent   = 0;
    };

    std::vector<LineLayout> layoutText(
        const std::string& utf8, int fontSize,
        bool bold, bool italic,
        float outlineW, int maxW);

    // Compositor
    void blendGlyph(std::vector<uint8_t>& canvas,
                     int canvasW, int canvasH,
                     const GlyphBitmap& glyph,
                     int x, int y,
                     uint32_t color, bool useOutline) noexcept;

    static uint32_t nextCodepoint(const char*& p, const char* end) noexcept;
    static uint8_t  alphaBlend(uint8_t dst, uint8_t src, uint8_t alpha) noexcept;

    FT_Library m_library = nullptr;
    FT_Face    m_face    = nullptr;

    std::unordered_map<CacheKey, GlyphBitmap, CacheKeyHash> m_glyphCache;

    static constexpr size_t kMaxCacheEntries = 2048;
};

} // namespace aurora::subtitle

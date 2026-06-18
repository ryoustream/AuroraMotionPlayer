#pragma once
#include "SubtitleEngine.h"
#include "FreeTypeRenderer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace aurora::subtitle {

/**
 * Aurora Motion Player — ASS / SSA Renderer (Session 8)
 *
 * Implements a practical subset of the ASS (Advanced SubStation Alpha)
 * tag language on top of FreeTypeRenderer, sufficient for the vast
 * majority of fansub/anime release styling.
 *
 * Supported override tags:
 *   \b1 \b0                bold on/off
 *   \i1 \i0                italic on/off
 *   \u1 \u0                underline (rendered as a drawn line)
 *   \s1 \s0                strikeout (rendered as a drawn line)
 *   \fnNAME                font name
 *   \fsN                   font size
 *   \c&HBBGGRR&  \1c..      primary colour (ASS uses BGR order)
 *   \2c \3c \4c             secondary / outline / shadow colour
 *   \alpha&HAA&  \1a..\4a   per-channel alpha
 *   \bordN                  outline width
 *   \shadN                  shadow depth
 *   \anN                    alignment (numpad 1-9)
 *   \posX,Y\                absolute position
 *   \move(x1,y1,x2,y2[,t1,t2])  linear move animation
 *   \fadIN,OUT  \fad(...)   fade in/out alpha animation
 *   \r[STYLE]               reset to style (or named style)
 *   \N                      hard line break
 *   \h                      hard space
 *
 * Styles are parsed from the [V4+ Styles] section of the .ass script
 * and dialogue lines from [Events].
 */
class ASSRenderer {
public:
    ASSRenderer();
    ~ASSRenderer();

    // ── Script loading ────────────────────────────────────────────────────────

    struct ASSStyle {
        std::string name        = "Default";
        std::string fontName    = "Arial";
        int         fontSize    = 20;
        uint32_t    primaryColor   = 0xFFFFFFFF; // RGBA
        uint32_t    secondaryColor = 0xFF0000FF;
        uint32_t    outlineColor   = 0xFF000000;
        uint32_t    shadowColor    = 0x80000000;
        bool        bold        = false;
        bool        italic      = false;
        bool        underline   = false;
        bool        strikeout   = false;
        float       scaleX      = 100.0f;
        float       scaleY      = 100.0f;
        float       spacing     = 0.0f;
        float       angle       = 0.0f;
        int         borderStyle = 1;
        float       outline     = 2.0f;
        float       shadow      = 2.0f;
        int         alignment   = 2;       // numpad layout
        int         marginL     = 10;
        int         marginR     = 10;
        int         marginV     = 20;
    };

    struct DialogueLine {
        double      startTime = 0.0;
        double      endTime   = 0.0;
        int         layer     = 0;
        std::string styleName = "Default";
        std::string actor;
        std::string text;       // raw text incl. override tags
        int         marginL    = 0;
        int         marginR    = 0;
        int         marginV    = 0;
    };

    /** Parse a full .ass/.ssa script (styles + dialogue events). */
    bool loadScript(const std::string& path);
    bool loadScriptText(const std::string& content);

    /** Convenience: parse only the style block from a string. */
    static std::unordered_map<std::string, ASSStyle> parseStyles(
        const std::string& scriptContent);

    /** Convenience: parse dialogue events from a string. */
    static std::vector<DialogueLine> parseEvents(
        const std::string& scriptContent);

    // ── Rendering ─────────────────────────────────────────────────────────────

    struct RenderedASS {
        std::vector<uint8_t> rgba;
        int x = 0, y = 0;
        int width = 0, height = 0;
    };

    /** Render all dialogue lines active at [timestamp]. */
    std::vector<RenderedASS> renderAt(double timestamp,
                                       int videoWidth, int videoHeight);

    void setFontDirectory(const std::string& dir) { m_fontDir = dir; }

    const std::vector<DialogueLine>& events() const noexcept { return m_events; }
    const std::unordered_map<std::string, ASSStyle>& styles() const noexcept { return m_styles; }

private:
    // ── Tag parsing ───────────────────────────────────────────────────────────

    struct InlineState {
        ASSStyle style;          // mutable copy, modified by override tags
        bool     hasPos   = false;
        float    posX = 0, posY = 0;
        bool     hasMove = false;
        float    moveX1=0, moveY1=0, moveX2=0, moveY2=0, moveT1=0, moveT2=0;
        bool     hasFade = false;
        int      fadeIn = 0, fadeOut = 0;
    };

    struct TextRun {
        std::string text;
        ASSStyle    style;       // resolved style for this run
    };

    // Strip and apply override tags; returns list of styled runs + position info
    std::vector<TextRun> parseOverrides(
        const std::string& text, const ASSStyle& baseStyle, InlineState& state);

    static uint32_t parseASSColor(const std::string& hex, uint32_t fallback);
    static void applyTag(const std::string& tag, InlineState& state,
                          const std::unordered_map<std::string, ASSStyle>& styles);

    float computeAnimatedAlpha(const InlineState& st, double t,
                                 double evStart, double evEnd) const;

    void computePosition(const InlineState& st, int alignment,
                          int videoW, int videoH,
                          int textW, int textH,
                          int marginL, int marginR, int marginV,
                          int& outX, int& outY) const;

    std::unordered_map<std::string, ASSStyle> m_styles;
    std::vector<DialogueLine>                 m_events;
    std::string                               m_fontDir;

    std::unique_ptr<FreeTypeRenderer> m_ftRenderer;
    std::unordered_map<std::string, bool> m_loadedFonts; // fontName -> loaded ok
};

} // namespace aurora::subtitle

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace aurora::subtitle {

enum class SubtitleFormat {
    Unknown,
    SRT,
    ASS,
    SSA,
    VTT,
    PGS,
    SUP,
    IDXSUB,
};

struct SubtitleStyle {
    std::string fontName     = "Arial";
    int         fontSize     = 24;
    uint32_t    primaryColor = 0xFFFFFF;   // ARGB
    uint32_t    outlineColor = 0x000000;
    uint32_t    shadowColor  = 0x80000000;
    float       outlineWidth = 2.0f;
    float       shadowDepth  = 2.0f;
    bool        bold         = false;
    bool        italic       = false;
    int         marginV      = 20;
    int         marginH      = 10;
    int         alignment    = 2;  // ASS alignment (numpad layout)
};

struct SubtitleEvent {
    double      startTime  = 0.0;  // seconds
    double      endTime    = 0.0;
    std::string text;
    SubtitleStyle style;
    bool        isForced   = false;
    // Bitmap data for PGS/image subs
    std::vector<uint8_t> bitmap;
    int         bitmapW    = 0;
    int         bitmapH    = 0;
    int         bitmapX    = 0;
    int         bitmapY    = 0;
};

class SubtitleParser {
public:
    static SubtitleFormat detectFormat(const std::string& path);
    static std::vector<SubtitleEvent> parseSRT(const std::string& path);
    static std::vector<SubtitleEvent> parseASS(const std::string& path);
    static std::vector<SubtitleEvent> parseVTT(const std::string& path);
    // PGS/SUP (bitmap) parsing
    static std::vector<SubtitleEvent> parsePGS(const std::string& path);
};

class SubtitleEngine {
public:
    using RenderCallback = std::function<void(const SubtitleEvent&)>;

    SubtitleEngine();
    ~SubtitleEngine() = default;

    bool loadFile(const std::string& path);
    bool loadStream(const std::vector<uint8_t>& data, SubtitleFormat fmt);
    void unload();

    // Returns subtitle event(s) active at given timestamp
    std::vector<const SubtitleEvent*> getActiveAt(double timestamp) const;

    void setDelay(double delaySeconds) noexcept { m_delay = delaySeconds; }
    double delay() const noexcept { return m_delay; }

    void setDefaultStyle(const SubtitleStyle& style) { m_defaultStyle = style; }
    const SubtitleStyle& defaultStyle() const noexcept { return m_defaultStyle; }

    size_t eventCount() const noexcept { return m_events.size(); }

private:
    std::vector<SubtitleEvent> m_events;
    SubtitleStyle               m_defaultStyle;
    double                      m_delay      = 0.0;
    SubtitleFormat              m_format     = SubtitleFormat::Unknown;
};

} // namespace aurora::subtitle

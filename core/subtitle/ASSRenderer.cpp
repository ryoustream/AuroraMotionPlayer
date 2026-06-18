#include "ASSRenderer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace aurora::subtitle {

namespace {
std::string trimStr(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

std::vector<std::string> splitStr(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) out.push_back(item);
    return out;
}

double parseASSTimestamp(const std::string& ts) {
    int h = 0, m = 0, s = 0, cs = 0;
    sscanf(ts.c_str(), "%d:%d:%d.%d", &h, &m, &s, &cs);
    return h * 3600.0 + m * 60.0 + s + cs / 100.0;
}
} // namespace

ASSRenderer::ASSRenderer()
    : m_ftRenderer(std::make_unique<FreeTypeRenderer>())
{
}

ASSRenderer::~ASSRenderer() = default;

// ── Color parsing ─────────────────────────────────────────────────────────────
// ASS colors are &HBBGGRR& (note: blue-green-red order, reversed from RGB)

uint32_t ASSRenderer::parseASSColor(const std::string& hexIn, uint32_t fallback)
{
    std::string hex = hexIn;
    // Strip &H ... & wrapper
    if (hex.size() >= 2 && (hex[0] == '&' || hex[0] == 'H')) {
        size_t start = hex.find_first_of("0123456789ABCDEFabcdef");
        size_t end   = hex.find('&', start == std::string::npos ? 0 : start);
        if (start == std::string::npos) return fallback;
        hex = (end == std::string::npos) ? hex.substr(start) : hex.substr(start, end - start);
    }
    if (hex.empty()) return fallback;

    uint32_t val = 0;
    try { val = static_cast<uint32_t>(std::stoul(hex, nullptr, 16)); }
    catch (...) { return fallback; }

    // val is 0x00BBGGRR (alpha may be packed in some contexts as 0xAABBGGRR)
    uint8_t bb = (val >> 16) & 0xFF;
    uint8_t gg = (val >>  8) & 0xFF;
    uint8_t rr =  val        & 0xFF;
    uint8_t aa = 0xFF; // default opaque; alpha handled separately via \alpha tags

    return (static_cast<uint32_t>(rr) << 24) |
           (static_cast<uint32_t>(gg) << 16) |
           (static_cast<uint32_t>(bb) <<  8) |
            static_cast<uint32_t>(aa);
}

// ── Style block parsing ──────────────────────────────────────────────────────

std::unordered_map<std::string, ASSRenderer::ASSStyle> ASSRenderer::parseStyles(
    const std::string& content)
{
    std::unordered_map<std::string, ASSStyle> styles;

    std::istringstream iss(content);
    std::string line;
    bool inStylesSection = false;
    std::vector<std::string> fields;

    while (std::getline(iss, line)) {
        line = trimStr(line);
        if (line.empty()) continue;

        if (line[0] == '[') {
            inStylesSection = (line.find("V4+ Styles") != std::string::npos ||
                               line.find("V4 Styles")  != std::string::npos);
            continue;
        }
        if (!inStylesSection) continue;

        if (line.rfind("Format:", 0) == 0) {
            std::string fmt = line.substr(7);
            fields.clear();
            for (auto& f : splitStr(fmt, ','))
                fields.push_back(trimStr(f));
            continue;
        }

        if (line.rfind("Style:", 0) == 0 && !fields.empty()) {
            std::string data = line.substr(6);
            auto values = splitStr(data, ',');

            ASSStyle st;
            for (size_t i = 0; i < fields.size() && i < values.size(); ++i) {
                std::string f = fields[i];
                std::string v = trimStr(values[i]);

                if (f == "Name")            st.name = v;
                else if (f == "Fontname")   st.fontName = v;
                else if (f == "Fontsize")   st.fontSize = std::atoi(v.c_str());
                else if (f == "PrimaryColour")   st.primaryColor   = parseASSColor(v, st.primaryColor);
                else if (f == "SecondaryColour") st.secondaryColor = parseASSColor(v, st.secondaryColor);
                else if (f == "OutlineColour")   st.outlineColor   = parseASSColor(v, st.outlineColor);
                else if (f == "BackColour")      st.shadowColor    = parseASSColor(v, st.shadowColor);
                else if (f == "Bold")       st.bold      = (std::atoi(v.c_str()) != 0);
                else if (f == "Italic")     st.italic    = (std::atoi(v.c_str()) != 0);
                else if (f == "Underline")  st.underline = (std::atoi(v.c_str()) != 0);
                else if (f == "StrikeOut")  st.strikeout = (std::atoi(v.c_str()) != 0);
                else if (f == "ScaleX")     st.scaleX  = static_cast<float>(std::atof(v.c_str()));
                else if (f == "ScaleY")     st.scaleY  = static_cast<float>(std::atof(v.c_str()));
                else if (f == "Spacing")    st.spacing = static_cast<float>(std::atof(v.c_str()));
                else if (f == "Angle")      st.angle   = static_cast<float>(std::atof(v.c_str()));
                else if (f == "BorderStyle")st.borderStyle = std::atoi(v.c_str());
                else if (f == "Outline")    st.outline = static_cast<float>(std::atof(v.c_str()));
                else if (f == "Shadow")     st.shadow  = static_cast<float>(std::atof(v.c_str()));
                else if (f == "Alignment")  st.alignment = std::atoi(v.c_str());
                else if (f == "MarginL")    st.marginL = std::atoi(v.c_str());
                else if (f == "MarginR")    st.marginR = std::atoi(v.c_str());
                else if (f == "MarginV")    st.marginV = std::atoi(v.c_str());
            }
            styles[st.name] = st;
        }
    }

    if (styles.find("Default") == styles.end()) {
        ASSStyle def;
        styles["Default"] = def;
    }
    return styles;
}

// ── Events block parsing ─────────────────────────────────────────────────────

std::vector<ASSRenderer::DialogueLine> ASSRenderer::parseEvents(
    const std::string& content)
{
    std::vector<DialogueLine> events;
    std::istringstream iss(content);
    std::string line;
    bool inEventsSection = false;
    std::vector<std::string> fields;

    while (std::getline(iss, line)) {
        line = trimStr(line);
        if (line.empty()) continue;

        if (line[0] == '[') {
            inEventsSection = (line.find("Events") != std::string::npos);
            continue;
        }
        if (!inEventsSection) continue;

        if (line.rfind("Format:", 0) == 0) {
            fields.clear();
            for (auto& f : splitStr(line.substr(7), ','))
                fields.push_back(trimStr(f));
            continue;
        }

        if (line.rfind("Dialogue:", 0) == 0 && !fields.empty()) {
            std::string data = line.substr(9);
            // Text field may contain commas — split only up to (fields.size()-1)
            std::vector<std::string> values;
            size_t pos = 0;
            for (size_t i = 0; i + 1 < fields.size(); ++i) {
                size_t comma = data.find(',', pos);
                if (comma == std::string::npos) break;
                values.push_back(data.substr(pos, comma - pos));
                pos = comma + 1;
            }
            values.push_back(data.substr(pos)); // remaining = Text

            DialogueLine dl;
            for (size_t i = 0; i < fields.size() && i < values.size(); ++i) {
                std::string f = fields[i];
                std::string v = trimStr(values[i]);

                if (f == "Layer")    dl.layer = std::atoi(v.c_str());
                else if (f == "Start")    dl.startTime = parseASSTimestamp(v);
                else if (f == "End")      dl.endTime   = parseASSTimestamp(v);
                else if (f == "Style")    dl.styleName = v;
                else if (f == "Name" || f == "Actor") dl.actor = v;
                else if (f == "MarginL")  dl.marginL = std::atoi(v.c_str());
                else if (f == "MarginR")  dl.marginR = std::atoi(v.c_str());
                else if (f == "MarginV")  dl.marginV = std::atoi(v.c_str());
                else if (f == "Text")     dl.text = v;
            }
            events.push_back(std::move(dl));
        }
    }
    return events;
}

// ── Script loading ────────────────────────────────────────────────────────────

bool ASSRenderer::loadScriptText(const std::string& content)
{
    m_styles = parseStyles(content);
    m_events = parseEvents(content);
    return !m_events.empty() || !m_styles.empty();
}

bool ASSRenderer::loadScript(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string content((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    return loadScriptText(content);
}

// ── Override tag parsing ─────────────────────────────────────────────────────

void ASSRenderer::applyTag(const std::string& tag, InlineState& state,
                             const std::unordered_map<std::string, ASSStyle>& styles)
{
    if (tag.empty()) return;

    auto startsWith = [&](const char* prefix) {
        return tag.rfind(prefix, 0) == 0;
    };

    if (startsWith("b"))  { state.style.bold      = (tag.size() > 1 && tag[1] == '1'); return; }
    if (startsWith("i"))  { state.style.italic    = (tag.size() > 1 && tag[1] == '1'); return; }
    if (startsWith("u"))  { state.style.underline = (tag.size() > 1 && tag[1] == '1'); return; }
    if (startsWith("s") && tag.size() > 1 && (tag[1] == '0' || tag[1] == '1')) {
        state.style.strikeout = (tag[1] == '1'); return;
    }

    if (startsWith("fn")) { state.style.fontName = tag.substr(2); return; }
    if (startsWith("fs")) { state.style.fontSize = std::atoi(tag.c_str() + 2); return; }

    if (startsWith("c") || startsWith("1c")) {
        size_t off = startsWith("1c") ? 2 : 1;
        state.style.primaryColor = parseASSColor(tag.substr(off), state.style.primaryColor);
        return;
    }
    if (startsWith("2c")) { state.style.secondaryColor = parseASSColor(tag.substr(2), state.style.secondaryColor); return; }
    if (startsWith("3c")) { state.style.outlineColor   = parseASSColor(tag.substr(2), state.style.outlineColor);   return; }
    if (startsWith("4c")) { state.style.shadowColor    = parseASSColor(tag.substr(2), state.style.shadowColor);    return; }

    if (startsWith("alpha")) {
        std::string hex = tag.substr(5);
        uint32_t a = parseASSColor(hex, 0xFF) & 0xFF;
        uint8_t alpha = 255 - static_cast<uint8_t>(a); // ASS alpha: 00=opaque, FF=transparent
        state.style.primaryColor = (state.style.primaryColor & 0xFFFFFF00) | alpha;
        return;
    }
    if (startsWith("1a")) {
        uint32_t a = parseASSColor(tag.substr(2), 0xFF) & 0xFF;
        uint8_t alpha = 255 - static_cast<uint8_t>(a);
        state.style.primaryColor = (state.style.primaryColor & 0xFFFFFF00) | alpha;
        return;
    }
    if (startsWith("3a")) {
        uint32_t a = parseASSColor(tag.substr(2), 0xFF) & 0xFF;
        uint8_t alpha = 255 - static_cast<uint8_t>(a);
        state.style.outlineColor = (state.style.outlineColor & 0xFFFFFF00) | alpha;
        return;
    }
    if (startsWith("4a")) {
        uint32_t a = parseASSColor(tag.substr(2), 0xFF) & 0xFF;
        uint8_t alpha = 255 - static_cast<uint8_t>(a);
        state.style.shadowColor = (state.style.shadowColor & 0xFFFFFF00) | alpha;
        return;
    }

    if (startsWith("bord")) { state.style.outline = static_cast<float>(std::atof(tag.c_str() + 4)); return; }
    if (startsWith("shad")) { state.style.shadow  = static_cast<float>(std::atof(tag.c_str() + 4)); return; }

    if (startsWith("an")) { state.style.alignment = std::atoi(tag.c_str() + 2); return; }

    if (startsWith("pos")) {
        float x = 0, y = 0;
        if (sscanf(tag.c_str() + 3, "(%f,%f)", &x, &y) == 2) {
            state.hasPos = true; state.posX = x; state.posY = y;
        }
        return;
    }

    if (startsWith("move")) {
        float x1, y1, x2, y2, t1 = 0, t2 = 0;
        int n = sscanf(tag.c_str() + 4, "(%f,%f,%f,%f,%f,%f)",
                       &x1, &y1, &x2, &y2, &t1, &t2);
        if (n >= 4) {
            state.hasMove = true;
            state.moveX1 = x1; state.moveY1 = y1;
            state.moveX2 = x2; state.moveY2 = y2;
            state.moveT1 = t1; state.moveT2 = t2;
        }
        return;
    }

    if (startsWith("fad")) {
        int in = 0, out = 0;
        if (sscanf(tag.c_str() + 3, "(%d,%d)", &in, &out) == 2 ||
            sscanf(tag.c_str() + 3, "e(%d,%d)", &in, &out) == 2) {
            state.hasFade = true; state.fadeIn = in; state.fadeOut = out;
        }
        return;
    }

    if (startsWith("r")) {
        std::string styleName = tag.substr(1);
        if (!styleName.empty()) {
            auto it = styles.find(styleName);
            if (it != styles.end()) state.style = it->second;
        }
        return;
    }
}

std::vector<ASSRenderer::TextRun> ASSRenderer::parseOverrides(
    const std::string& text, const ASSStyle& baseStyle, InlineState& state)
{
    std::vector<TextRun> runs;
    state.style = baseStyle;

    std::string current;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            // Flush current run
            if (!current.empty()) {
                runs.push_back({current, state.style});
                current.clear();
            }
            size_t close = text.find('}', i);
            if (close == std::string::npos) break;
            std::string tagBlock = text.substr(i + 1, close - i - 1);
            // Tags separated by backslash
            for (auto& tag : splitStr(tagBlock, '\\')) {
                applyTag(trimStr(tag), state, m_styles);
            }
            i = close + 1;
        } else if (text[i] == '\\' && i + 1 < text.size() &&
                   (text[i+1] == 'N' || text[i+1] == 'n')) {
            current += '\n';
            i += 2;
        } else if (text[i] == '\\' && i + 1 < text.size() && text[i+1] == 'h') {
            current += ' ';
            i += 2;
        } else {
            current += text[i++];
        }
    }
    if (!current.empty()) runs.push_back({current, state.style});
    return runs;
}

// ── Position computation (ASS numpad alignment) ──────────────────────────────

void ASSRenderer::computePosition(
    const InlineState& st, int alignment,
    int videoW, int videoH,
    int textW, int textH,
    int marginL, int marginR, int marginV,
    int& outX, int& outY) const
{
    if (st.hasPos) {
        outX = static_cast<int>(st.posX);
        outY = static_cast<int>(st.posY);
        return;
    }

    // Horizontal: 1/4/7 = left, 2/5/8 = center, 3/6/9 = right
    int col = (alignment - 1) % 3;       // 0=left,1=center,2=right
    int row = (alignment - 1) / 3;       // 0=bottom,1=mid,2=top (ASS numpad)

    switch (col) {
        case 0: outX = marginL; break;
        case 1: outX = (videoW - textW) / 2; break;
        case 2: outX = videoW - marginR - textW; break;
    }
    switch (row) {
        case 0: outY = videoH - marginV - textH; break;  // bottom
        case 1: outY = (videoH - textH) / 2; break;       // middle
        case 2: outY = marginV; break;                    // top
    }
}

float ASSRenderer::computeAnimatedAlpha(
    const InlineState& st, double t, double evStart, double evEnd) const
{
    if (!st.hasFade) return 1.0f;
    double elapsed   = (t - evStart) * 1000.0; // ms
    double remaining = (evEnd - t)   * 1000.0;

    float alpha = 1.0f;
    if (st.fadeIn > 0 && elapsed < st.fadeIn)
        alpha = static_cast<float>(elapsed / st.fadeIn);
    if (st.fadeOut > 0 && remaining < st.fadeOut)
        alpha = std::min(alpha, static_cast<float>(remaining / st.fadeOut));
    return std::clamp(alpha, 0.0f, 1.0f);
}

// ── Main render ───────────────────────────────────────────────────────────────

std::vector<ASSRenderer::RenderedASS> ASSRenderer::renderAt(
    double timestamp, int videoWidth, int videoHeight)
{
    std::vector<RenderedASS> results;

    for (const auto& ev : m_events) {
        if (timestamp < ev.startTime || timestamp > ev.endTime) continue;

        auto styleIt = m_styles.find(ev.styleName);
        ASSStyle base = (styleIt != m_styles.end()) ? styleIt->second : ASSStyle{};

        InlineState state;
        auto runs = parseOverrides(ev.text, base, state);
        if (runs.empty()) continue;

        // Load font for the resolved style (lazy, cached by name)
        const std::string& fontName = runs[0].style.fontName;
        if (m_loadedFonts.find(fontName) == m_loadedFonts.end()) {
            bool ok = m_ftRenderer->loadSystemFont(fontName,
                                                     runs[0].style.bold,
                                                     runs[0].style.italic);
            if (!ok) ok = m_ftRenderer->loadSystemFont("DejaVuSans");
            m_loadedFonts[fontName] = ok;
        }
        if (!m_loadedFonts[fontName]) continue;

        // Concatenate runs into one render call for now (mixed-style
        // requires a richer multi-run renderer; this handles dominant style)
        std::string combinedText;
        for (auto& r : runs) combinedText += r.text;

        FreeTypeRenderer::RenderParams rp;
        rp.text          = combinedText;
        rp.fontSize       = runs[0].style.fontSize;
        rp.primaryColor   = runs[0].style.primaryColor;
        rp.outlineColor   = runs[0].style.outlineColor;
        rp.shadowColor    = runs[0].style.shadowColor;
        rp.outlineWidth   = runs[0].style.outline;
        rp.shadowOffX     = runs[0].style.shadow;
        rp.shadowOffY     = runs[0].style.shadow;
        rp.bold           = runs[0].style.bold;
        rp.italic         = runs[0].style.italic;
        rp.maxWidthPx     = videoWidth - ev.marginL - ev.marginR;
        rp.alignment      = runs[0].style.alignment;

        auto rendered = m_ftRenderer->render(rp);
        if (rendered.rgba.empty()) continue;

        // Apply fade alpha
        float alpha = computeAnimatedAlpha(state, timestamp, ev.startTime, ev.endTime);
        if (alpha < 0.999f) {
            for (size_t i = 3; i < rendered.rgba.size(); i += 4) {
                rendered.rgba[i] = static_cast<uint8_t>(rendered.rgba[i] * alpha);
            }
        }

        int x, y;
        computePosition(state, runs[0].style.alignment,
                         videoWidth, videoHeight,
                         rendered.width, rendered.height,
                         ev.marginL > 0 ? ev.marginL : runs[0].style.marginL,
                         ev.marginR > 0 ? ev.marginR : runs[0].style.marginR,
                         ev.marginV > 0 ? ev.marginV : runs[0].style.marginV,
                         x, y);

        RenderedASS out;
        out.rgba   = std::move(rendered.rgba);
        out.width  = rendered.width;
        out.height = rendered.height;
        out.x      = x;
        out.y      = y;
        results.push_back(std::move(out));
    }
    return results;
}

} // namespace aurora::subtitle

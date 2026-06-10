#include "SubtitleEngine.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace aurora::subtitle {

// ── Helper: trim whitespace ───────────────────────────────────────────────────
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// ── Timestamp parsers ─────────────────────────────────────────────────────────
static double parseSRTTime(const std::string& ts) {
    // "HH:MM:SS,mmm"
    int h = 0, m = 0, s = 0, ms = 0;
    sscanf(ts.c_str(), "%d:%d:%d,%d", &h, &m, &s, &ms);
    return h * 3600.0 + m * 60.0 + s + ms / 1000.0;
}

static double parseASSTime(const std::string& ts) {
    // "H:MM:SS.cc"
    int h = 0, m = 0, s = 0, cs = 0;
    sscanf(ts.c_str(), "%d:%d:%d.%d", &h, &m, &s, &cs);
    return h * 3600.0 + m * 60.0 + s + cs / 100.0;
}

// ── SubtitleParser ────────────────────────────────────────────────────────────
SubtitleFormat SubtitleParser::detectFormat(const std::string& path) {
    std::string ext;
    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        ext = path.substr(dot + 1);
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    }
    if (ext == "srt")  return SubtitleFormat::SRT;
    if (ext == "ass")  return SubtitleFormat::ASS;
    if (ext == "ssa")  return SubtitleFormat::SSA;
    if (ext == "vtt")  return SubtitleFormat::VTT;
    if (ext == "sup" || ext == "pgs") return SubtitleFormat::PGS;
    if (ext == "idx")  return SubtitleFormat::IDXSUB;
    return SubtitleFormat::Unknown;
}

std::vector<SubtitleEvent> SubtitleParser::parseSRT(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream file(path);
    if (!file.is_open()) return events;

    std::string line;
    while (std::getline(file, line)) {
        // Skip sequence number
        line = trim(line);
        if (line.empty() || std::isdigit(static_cast<unsigned char>(line[0]))) {
            // Next line: timestamps
            std::string tsLine;
            if (!std::getline(file, tsLine)) break;
            tsLine = trim(tsLine);
            if (tsLine.find("-->") == std::string::npos) continue;

            auto sep = tsLine.find("-->");
            std::string start = trim(tsLine.substr(0, sep));
            std::string end   = trim(tsLine.substr(sep + 3));

            SubtitleEvent ev;
            ev.startTime = parseSRTTime(start);
            ev.endTime   = parseSRTTime(end);

            // Collect text lines until blank line
            std::string textLine;
            while (std::getline(file, textLine)) {
                textLine = trim(textLine);
                if (textLine.empty()) break;
                if (!ev.text.empty()) ev.text += '\n';
                ev.text += textLine;
            }

            // Strip basic HTML tags (<b>, <i>, <u>, <font ...>)
            ev.text = std::regex_replace(ev.text, std::regex("<[^>]+>"), "");

            if (!ev.text.empty()) events.push_back(std::move(ev));
        }
    }
    return events;
}

std::vector<SubtitleEvent> SubtitleParser::parseASS(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream file(path);
    if (!file.is_open()) return events;

    std::string line;
    bool inEvents = false;
    std::vector<std::string> format;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line == "[Events]") { inEvents = true; continue; }
        if (line.empty() || line[0] == ';') continue;
        if (!inEvents) continue;

        if (line.substr(0, 7) == "Format:") {
            std::istringstream ss(line.substr(7));
            std::string tok;
            while (std::getline(ss, tok, ',')) format.push_back(trim(tok));
            continue;
        }
        if (line.substr(0, 9) != "Dialogue:") continue;

        // Split dialogue by comma (max N-1 splits for last "Text" field)
        std::vector<std::string> fields;
        std::istringstream ss(line.substr(9));
        std::string tok;
        int limit = static_cast<int>(format.size()) - 1;
        int count = 0;
        while (count < limit && std::getline(ss, tok, ',')) {
            fields.push_back(trim(tok));
            ++count;
        }
        // Remaining is text
        std::string rest;
        std::getline(ss, rest);
        fields.push_back(rest);

        // Map format fields
        auto idx = [&](const std::string& name) -> int {
            for (int i = 0; i < (int)format.size(); ++i)
                if (format[i] == name) return i;
            return -1;
        };

        int iStart = idx("Start"), iEnd = idx("End"), iText = idx("Text");
        if (iStart < 0 || iEnd < 0 || iText < 0) continue;
        if (iStart >= (int)fields.size() || iEnd >= (int)fields.size()) continue;

        SubtitleEvent ev;
        ev.startTime = parseASSTime(fields[iStart]);
        ev.endTime   = parseASSTime(fields[iEnd]);
        ev.text      = (iText < (int)fields.size()) ? fields[iText] : "";

        // Strip ASS override tags: {\...}
        ev.text = std::regex_replace(ev.text, std::regex("\\{[^}]*\\}"), "");
        // Replace \N with newline
        ev.text = std::regex_replace(ev.text, std::regex("\\\\N"), "\n");
        ev.text = std::regex_replace(ev.text, std::regex("\\\\n"), "\n");

        if (!ev.text.empty()) events.push_back(std::move(ev));
    }
    return events;
}

std::vector<SubtitleEvent> SubtitleParser::parseVTT(const std::string& path) {
    // VTT is similar to SRT with different timestamp format
    std::vector<SubtitleEvent> events;
    std::ifstream file(path);
    if (!file.is_open()) return events;
    // Skip WEBVTT header
    std::string line;
    std::getline(file, line); // WEBVTT
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.find("-->") == std::string::npos) continue;
        auto sep = line.find("-->");
        std::string start = trim(line.substr(0, sep));
        std::string end   = trim(line.substr(sep + 3));
        // VTT uses "HH:MM:SS.mmm" or "MM:SS.mmm"
        auto parseVTTTime = [](const std::string& ts) -> double {
            double h = 0, m = 0, s = 0;
            int cnt = sscanf(ts.c_str(), "%lf:%lf:%lf", &h, &m, &s);
            if (cnt == 2) { s = m; m = h; h = 0; }
            return h * 3600.0 + m * 60.0 + s;
        };
        SubtitleEvent ev;
        ev.startTime = parseVTTTime(start);
        ev.endTime   = parseVTTTime(end);
        std::string textLine;
        while (std::getline(file, textLine)) {
            textLine = trim(textLine);
            if (textLine.empty()) break;
            if (!ev.text.empty()) ev.text += '\n';
            ev.text += textLine;
        }
        ev.text = std::regex_replace(ev.text, std::regex("<[^>]+>"), "");
        if (!ev.text.empty()) events.push_back(std::move(ev));
    }
    return events;
}

std::vector<SubtitleEvent> SubtitleParser::parsePGS(const std::string& /*path*/) {
    // PGS/SUP bitmap subtitle parsing — requires binary parsing of PCS/ODS/WDS/END segments
    // Full implementation parses HDMV PGS specification (Blu-ray)
    return {};
}

// ── SubtitleEngine ────────────────────────────────────────────────────────────
SubtitleEngine::SubtitleEngine() = default;

bool SubtitleEngine::loadFile(const std::string& path) {
    m_format = SubtitleParser::detectFormat(path);
    switch (m_format) {
    case SubtitleFormat::SRT:                m_events = SubtitleParser::parseSRT(path); break;
    case SubtitleFormat::ASS:
    case SubtitleFormat::SSA:               m_events = SubtitleParser::parseASS(path); break;
    case SubtitleFormat::VTT:               m_events = SubtitleParser::parseVTT(path); break;
    case SubtitleFormat::PGS:
    case SubtitleFormat::SUP:               m_events = SubtitleParser::parsePGS(path); break;
    default:                                return false;
    }
    // Sort by start time
    std::sort(m_events.begin(), m_events.end(),
              [](const SubtitleEvent& a, const SubtitleEvent& b) {
                  return a.startTime < b.startTime;
              });
    return !m_events.empty();
}

bool SubtitleEngine::loadStream(const std::vector<uint8_t>& /*data*/, SubtitleFormat /*fmt*/) {
    // Write to temp file and parse — or parse directly from memory
    return false; // TODO
}

void SubtitleEngine::unload() {
    m_events.clear();
    m_format = SubtitleFormat::Unknown;
}

std::vector<const SubtitleEvent*> SubtitleEngine::getActiveAt(double timestamp) const {
    double t = timestamp + m_delay;
    std::vector<const SubtitleEvent*> active;
    for (const auto& ev : m_events) {
        if (ev.startTime <= t && ev.endTime >= t)
            active.push_back(&ev);
    }
    return active;
}

} // namespace aurora::subtitle

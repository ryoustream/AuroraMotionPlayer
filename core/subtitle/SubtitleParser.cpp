// SubtitleParser.cpp — full parser implementations for SRT, ASS/SSA, VTT, PGS
// This file complements SubtitleEngine.cpp
#include "SubtitleEngine.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace aurora::subtitle {

// ── Helper ────────────────────────────────────────────────────────────────────
static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    auto b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// Strip ASS override tags: {\an8}, {\b1}, {\i1}, etc.
static std::string stripASSTags(const std::string& s) {
    std::string out;
    bool inTag = false;
    for (char c : s) {
        if (c == '{') { inTag = true; continue; }
        if (c == '}') { inTag = false; continue; }
        if (!inTag) out += c;
    }
    // Replace \N and \n with space
    std::string result;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '\\' && i + 1 < out.size()
            && (out[i+1] == 'N' || out[i+1] == 'n')) {
            result += '\n';
            ++i;
        } else {
            result += out[i];
        }
    }
    return result;
}

static double parseSRTTime(const std::string& ts) {
    int h = 0, m = 0, s = 0, ms = 0;
    sscanf(ts.c_str(), "%d:%d:%d,%d", &h, &m, &s, &ms);
    return h * 3600.0 + m * 60.0 + s + ms / 1000.0;
}

static double parseASSTime(const std::string& ts) {
    int h = 0, m = 0, s = 0, cs = 0;
    sscanf(ts.c_str(), "%d:%d:%d.%d", &h, &m, &s, &cs);
    return h * 3600.0 + m * 60.0 + s + cs / 100.0;
}

static double parseVTTTime(const std::string& ts) {
    // "HH:MM:SS.mmm" or "MM:SS.mmm"
    int h = 0, m = 0, s = 0, ms = 0;
    if (std::count(ts.begin(), ts.end(), ':') == 2)
        sscanf(ts.c_str(), "%d:%d:%d.%d", &h, &m, &s, &ms);
    else
        sscanf(ts.c_str(), "%d:%d.%d", &m, &s, &ms);
    return h * 3600.0 + m * 60.0 + s + ms / 1000.0;
}

// ── SRT Parser ────────────────────────────────────────────────────────────────
std::vector<SubtitleEvent> SubtitleParser::parseSRT(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream f(path);
    if (!f.is_open()) return events;

    std::string line;
    enum State { SEEK_NUM, SEEK_TIME, SEEK_TEXT } state = SEEK_NUM;
    SubtitleEvent cur;
    std::string textAcc;

    auto flush = [&]() {
        if (!textAcc.empty()) {
            cur.text = trim(textAcc);
            if (cur.endTime > cur.startTime)
                events.push_back(cur);
        }
        cur = {};
        textAcc.clear();
    };

    while (std::getline(f, line)) {
        line = trim(line);
        switch (state) {
        case SEEK_NUM:
            if (!line.empty() && std::isdigit(static_cast<unsigned char>(line[0])))
                state = SEEK_TIME;
            break;
        case SEEK_TIME: {
            // "00:00:01,000 --> 00:00:04,000"
            auto arrowPos = line.find("-->");
            if (arrowPos != std::string::npos) {
                cur.startTime = parseSRTTime(trim(line.substr(0, arrowPos)));
                cur.endTime   = parseSRTTime(trim(line.substr(arrowPos + 3)));
                state = SEEK_TEXT;
            }
            break;
        }
        case SEEK_TEXT:
            if (line.empty()) {
                flush();
                state = SEEK_NUM;
            } else {
                if (!textAcc.empty()) textAcc += '\n';
                textAcc += line;
            }
            break;
        }
    }
    flush();  // last subtitle
    return events;
}

// ── ASS / SSA Parser ──────────────────────────────────────────────────────────
std::vector<SubtitleEvent> SubtitleParser::parseASS(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream f(path);
    if (!f.is_open()) return events;

    // Discover column order from [Events] Format line
    std::vector<std::string> columns;
    bool inEvents = false;
    std::string line;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line == "[Events]") { inEvents = true; continue; }
        if (line.empty() || line[0] == '[') { if (inEvents) break; continue; }
        if (!inEvents) continue;

        if (line.substr(0, 7) == "Format:") {
            std::istringstream ss(line.substr(7));
            std::string col;
            while (std::getline(ss, col, ','))
                columns.push_back(trim(col));
        } else if (line.substr(0, 9) == "Dialogue:") {
            // Split on comma up to columns.size()-1, rest is text
            std::string data = line.substr(9);
            std::vector<std::string> fields;
            size_t pos = 0;
            int nCols = static_cast<int>(columns.size());
            for (int i = 0; i < nCols - 1 && pos < data.size(); ++i) {
                size_t comma = data.find(',', pos);
                if (comma == std::string::npos) { fields.push_back(data.substr(pos)); pos = data.size(); }
                else { fields.push_back(data.substr(pos, comma - pos)); pos = comma + 1; }
            }
            if (pos < data.size()) fields.push_back(data.substr(pos));

            auto idx = [&](const std::string& name) -> int {
                for (int i = 0; i < (int)columns.size(); ++i)
                    if (columns[i] == name) return i;
                return -1;
            };

            int startIdx = idx("Start"), endIdx = idx("End"), textIdx = idx("Text");
            if (startIdx < 0) startIdx = 1;
            if (endIdx < 0)   endIdx   = 2;
            if (textIdx < 0)  textIdx  = (int)fields.size() - 1;

            SubtitleEvent ev;
            if (startIdx < (int)fields.size()) ev.startTime = parseASSTime(trim(fields[startIdx]));
            if (endIdx   < (int)fields.size()) ev.endTime   = parseASSTime(trim(fields[endIdx]));
            if (textIdx  < (int)fields.size()) ev.text      = stripASSTags(fields[textIdx]);

            if (ev.endTime > ev.startTime && !ev.text.empty())
                events.push_back(ev);
        }
    }
    return events;
}

// ── WebVTT Parser ─────────────────────────────────────────────────────────────
std::vector<SubtitleEvent> SubtitleParser::parseVTT(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream f(path);
    if (!f.is_open()) return events;

    std::string line;
    SubtitleEvent cur;
    std::string textAcc;
    bool hasTime = false;

    auto flush = [&]() {
        if (hasTime && !textAcc.empty()) {
            cur.text = trim(textAcc);
            events.push_back(cur);
        }
        cur = {}; textAcc.clear(); hasTime = false;
    };

    // Skip header line
    std::getline(f, line);

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) { flush(); continue; }
        // Skip NOTE blocks
        if (line.substr(0, 4) == "NOTE") continue;

        auto arrowPos = line.find("-->");
        if (arrowPos != std::string::npos) {
            // Parse time line; may have positioning tags after times
            std::string timePart = line;
            cur.startTime = parseVTTTime(trim(timePart.substr(0, arrowPos)));
            // End time is up to first space after arrow
            std::string after = trim(timePart.substr(arrowPos + 3));
            size_t sp = after.find(' ');
            cur.endTime = parseVTTTime(sp == std::string::npos ? after : after.substr(0, sp));
            hasTime = true;
        } else if (hasTime) {
            // Strip <b>, <i>, <c>, <v> tags
            std::string clean;
            bool inHtml = false;
            for (char c : line) {
                if (c == '<') { inHtml = true; continue; }
                if (c == '>') { inHtml = false; continue; }
                if (!inHtml) clean += c;
            }
            if (!textAcc.empty()) textAcc += '\n';
            textAcc += clean;
        }
    }
    flush();
    return events;
}

// ── PGS / SUP Parser ─────────────────────────────────────────────────────────
// Minimal PGS reader: parses Presentation Composition Segment to extract
// bitmap dimensions and timing. Full decode requires ODS palette processing.
std::vector<SubtitleEvent> SubtitleParser::parsePGS(const std::string& path) {
    std::vector<SubtitleEvent> events;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return events;

    // PGS magic: 0x5047 ("PG")
    while (f.good()) {
        uint8_t magic[2];
        if (!f.read(reinterpret_cast<char*>(magic), 2)) break;
        if (magic[0] != 0x50 || magic[1] != 0x47) break;

        // PTS (4 bytes), DTS (4 bytes)
        uint8_t timebuf[8];
        if (!f.read(reinterpret_cast<char*>(timebuf), 8)) break;
        uint32_t pts = (timebuf[0] << 24) | (timebuf[1] << 16) |
                       (timebuf[2] << 8)  |  timebuf[3];

        uint8_t segType;
        if (!f.read(reinterpret_cast<char*>(&segType), 1)) break;

        uint8_t lenBuf[2];
        if (!f.read(reinterpret_cast<char*>(lenBuf), 2)) break;
        uint16_t segLen = (lenBuf[0] << 8) | lenBuf[1];

        std::vector<uint8_t> segData(segLen);
        if (!f.read(reinterpret_cast<char*>(segData.data()), segLen)) break;

        // Segment type 0x16 = Presentation Composition Segment
        if (segType == 0x16 && segLen >= 11) {
            SubtitleEvent ev;
            ev.startTime = pts / 90000.0;  // 90 kHz clock
            ev.endTime   = ev.startTime + 3.0;  // placeholder
            // Width/height at offset 0,2; comp_state at 6
            ev.bitmapW = (segData[0] << 8) | segData[1];
            ev.bitmapH = (segData[2] << 8) | segData[3];
            ev.text    = "[PGS Subtitle]";
            events.push_back(ev);
        }
    }
    return events;
}

} // namespace aurora::subtitle

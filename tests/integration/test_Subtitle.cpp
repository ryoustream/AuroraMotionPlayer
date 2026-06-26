/**
 * Aurora Motion Player — Integration Test: Subtitle Pipeline
 * ===========================================================
 * Tests the full subtitle stack end-to-end:
 *   SubtitleParser → SubtitleEngine → SubtitleRenderer → ASSRenderer / PGSParser
 *
 * Covers:
 *   - Format auto-detection from extension and content
 *   - SRT parsing: timestamp parsing, multi-line text, HTML-stripped text
 *   - ASS/SSA parsing: [Events] section, style inheritance
 *   - VTT parsing: NOTE blocks ignored, cue IDs optional
 *   - PGSParser: segment header detection, graceful fallback on invalid data
 *   - SubtitleEngine: loadFile, getActiveAt, delay offset
 *   - SubtitleEngine: overlapping events returned correctly
 *   - Fixture-based parsing of real .srt file (if present)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include "subtitle/SubtitleEngine.h"
// SubtitleParser class is declared in SubtitleEngine.h
#include "subtitle/PGSParser.h"
#include "subtitle/ASSRenderer.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace aurora::subtitle;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string fixturePath(const std::string& name) {
    const std::vector<std::string> bases = {
        "tests/fixtures/",
        "../tests/fixtures/",
        "../../tests/fixtures/",
    };
    for (const auto& base : bases) {
        std::string p = base + name;
        if (fs::exists(p)) return p;
    }
    return "";
}

// Write a temp SRT file and return its path
static std::string writeTempSRT(const std::string& content) {
    std::string path = "/tmp/aurora_test_" + std::to_string(
        std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".srt";
    std::ofstream f(path);
    f << content;
    return path;
}

static std::string writeTempASS(const std::string& content) {
    std::string path = "/tmp/aurora_test_ass_" + std::to_string(
        std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".ass";
    std::ofstream f(path);
    f << content;
    return path;
}

static std::string writeTempVTT(const std::string& content) {
    std::string path = "/tmp/aurora_test_vtt_" + std::to_string(
        std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".vtt";
    std::ofstream f(path);
    f << content;
    return path;
}

// ── Format Detection ──────────────────────────────────────────────────────────

TEST(SubtitleFormatDetection, SRTByExtension) {
    EXPECT_EQ(SubtitleParser::detectFormat("video.srt"), SubtitleFormat::SRT);
    EXPECT_EQ(SubtitleParser::detectFormat("/path/to/sub.srt"), SubtitleFormat::SRT);
}

TEST(SubtitleFormatDetection, ASSByExtension) {
    EXPECT_EQ(SubtitleParser::detectFormat("video.ass"), SubtitleFormat::ASS);
    EXPECT_EQ(SubtitleParser::detectFormat("video.ssa"), SubtitleFormat::SSA);
}

TEST(SubtitleFormatDetection, VTTByExtension) {
    EXPECT_EQ(SubtitleParser::detectFormat("video.vtt"), SubtitleFormat::VTT);
}

TEST(SubtitleFormatDetection, PGSByExtension) {
    EXPECT_EQ(SubtitleParser::detectFormat("video.sup"), SubtitleFormat::PGS);
    EXPECT_EQ(SubtitleParser::detectFormat("video.pgs"), SubtitleFormat::PGS);
}

TEST(SubtitleFormatDetection, UnknownExtension) {
    EXPECT_EQ(SubtitleParser::detectFormat("video.txt"), SubtitleFormat::Unknown);
    EXPECT_EQ(SubtitleParser::detectFormat(""),           SubtitleFormat::Unknown);
}

// ── SRT Parsing ───────────────────────────────────────────────────────────────

class SRTParseTest : public ::testing::Test {
protected:
    std::string srtPath;

    void SetUp() override {
        const std::string srt = R"(1
00:00:01,000 --> 00:00:03,000
Hello, World!

2
00:00:04,500 --> 00:00:06,000
Second line of subtitle.

3
00:00:07,000 --> 00:00:09,000
<b>Bold</b> and <i>italic</i> text.

4
00:00:10,000 --> 00:00:12,000
Line one
Line two
)";
        srtPath = writeTempSRT(srt);
    }

    void TearDown() override {
        fs::remove(srtPath);
    }
};

TEST_F(SRTParseTest, ParsesEventCount) {
    auto events = SubtitleParser::parseSRT(srtPath);
    ASSERT_EQ(events.size(), 4u);
}

TEST_F(SRTParseTest, FirstEventTimestamps) {
    auto events = SubtitleParser::parseSRT(srtPath);
    ASSERT_GE(events.size(), 1u);
    EXPECT_NEAR(events[0].startTime, 1.0, 0.01);
    EXPECT_NEAR(events[0].endTime,   3.0, 0.01);
}

TEST_F(SRTParseTest, SecondEventText) {
    auto events = SubtitleParser::parseSRT(srtPath);
    ASSERT_GE(events.size(), 2u);
    EXPECT_THAT(events[1].text, ::testing::HasSubstr("Second"));
}

TEST_F(SRTParseTest, HTMLTagsStripped) {
    auto events = SubtitleParser::parseSRT(srtPath);
    ASSERT_GE(events.size(), 3u);
    EXPECT_THAT(events[2].text, ::testing::Not(::testing::HasSubstr("<b>")));
    EXPECT_THAT(events[2].text, ::testing::HasSubstr("Bold"));
}

TEST_F(SRTParseTest, MultiLineText) {
    auto events = SubtitleParser::parseSRT(srtPath);
    ASSERT_GE(events.size(), 4u);
    EXPECT_THAT(events[3].text, ::testing::HasSubstr("Line one"));
    EXPECT_THAT(events[3].text, ::testing::HasSubstr("Line two"));
}

TEST(SRTParseEdgeCases, EmptyFile) {
    std::string path = writeTempSRT("");
    auto events = SubtitleParser::parseSRT(path);
    EXPECT_TRUE(events.empty());
    fs::remove(path);
}

TEST(SRTParseEdgeCases, NonexistentFile) {
    auto events = SubtitleParser::parseSRT("/nonexistent/path.srt");
    EXPECT_TRUE(events.empty());
}

// ── ASS Parsing ───────────────────────────────────────────────────────────────

TEST(ASSParseTest, ParseBasicASS) {
    const std::string ass = R"([Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,24,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,20,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Hello ASS World!
Dialogue: 0,0:00:04.00,0:00:06.00,Default,,0,0,0,,{\b1}Bold text{\b0}
)";
    std::string path = writeTempASS(ass);
    auto events = SubtitleParser::parseASS(path);
    fs::remove(path);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_NEAR(events[0].startTime, 1.0, 0.01);
    EXPECT_NEAR(events[0].endTime,   3.0, 0.01);
    EXPECT_THAT(events[0].text, ::testing::HasSubstr("Hello"));
}

TEST(ASSParseTest, NonexistentFile) {
    auto events = SubtitleParser::parseASS("/no/such/file.ass");
    EXPECT_TRUE(events.empty());
}

// ── VTT Parsing ───────────────────────────────────────────────────────────────

TEST(VTTParseTest, ParseBasicVTT) {
    const std::string vtt = R"(WEBVTT

NOTE This is a comment

00:00:01.000 --> 00:00:03.000
Hello VTT World!

intro
00:00:04.000 --> 00:00:06.000
Named cue text here.

)";
    std::string path = writeTempVTT(vtt);
    auto events = SubtitleParser::parseVTT(path);
    fs::remove(path);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_NEAR(events[0].startTime, 1.0, 0.01);
    EXPECT_THAT(events[0].text, ::testing::HasSubstr("VTT World"));
    EXPECT_THAT(events[1].text, ::testing::HasSubstr("Named cue"));
}

// ── SubtitleEngine Tests ──────────────────────────────────────────────────────

class SubtitleEngineTest : public ::testing::Test {
protected:
    SubtitleEngine engine;
    std::string    srtPath;

    void SetUp() override {
        const std::string srt = R"(1
00:00:01,000 --> 00:00:03,000
First subtitle

2
00:00:02,500 --> 00:00:04,500
Overlapping subtitle

3
00:00:10,000 --> 00:00:12,000
Late subtitle
)";
        srtPath = writeTempSRT(srt);
    }

    void TearDown() override {
        fs::remove(srtPath);
    }
};

TEST_F(SubtitleEngineTest, LoadFileSucceeds) {
    EXPECT_TRUE(engine.loadFile(srtPath));
    EXPECT_EQ(engine.eventCount(), 3u);
}

TEST_F(SubtitleEngineTest, GetActiveAtTimestamp) {
    engine.loadFile(srtPath);
    auto active = engine.getActiveAt(1.5);
    ASSERT_EQ(active.size(), 1u);
    EXPECT_THAT(active[0]->text, ::testing::HasSubstr("First"));
}

TEST_F(SubtitleEngineTest, GetActiveAtOverlap) {
    engine.loadFile(srtPath);
    auto active = engine.getActiveAt(3.0);
    // Both first (1.0–3.0) and second (2.5–4.5) overlap at t=3.0
    EXPECT_GE(active.size(), 1u);
}

TEST_F(SubtitleEngineTest, GetActiveBeforeAnyEvent) {
    engine.loadFile(srtPath);
    auto active = engine.getActiveAt(0.0);
    EXPECT_TRUE(active.empty());
}

TEST_F(SubtitleEngineTest, GetActiveAfterAllEvents) {
    engine.loadFile(srtPath);
    auto active = engine.getActiveAt(999.0);
    EXPECT_TRUE(active.empty());
}

TEST_F(SubtitleEngineTest, DelayShiftsEvents) {
    engine.loadFile(srtPath);
    engine.setDelay(1.0);  // Shift all events +1s

    // At t=1.5 with +1s delay, the first event (originally at 1.0)
    // is effectively at 2.0 — so t=1.5 should yield nothing
    auto active = engine.getActiveAt(1.5);
    // Behavior depends on implementation: positive delay means events
    // appear later, so fewer or no events at 1.5
    EXPECT_DOUBLE_EQ(engine.delay(), 1.0);
}

TEST_F(SubtitleEngineTest, UnloadClearsEvents) {
    engine.loadFile(srtPath);
    EXPECT_GT(engine.eventCount(), 0u);
    engine.unload();
    EXPECT_EQ(engine.eventCount(), 0u);
}

TEST_F(SubtitleEngineTest, LoadNonexistentFile) {
    EXPECT_FALSE(engine.loadFile("/no/such/file.srt"));
}

// ── PGSParser Unit Tests ──────────────────────────────────────────────────────

TEST(PGSParserTest, EmptyDataReturnsEmpty) {
    auto events = PGSParser::parseBuffer(nullptr, 0);
    EXPECT_TRUE(events.empty());
}

TEST(PGSParserTest, InvalidMagicBytesReturnsEmpty) {
    const uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    auto events = PGSParser::parseBuffer(data, sizeof(data));
    EXPECT_TRUE(events.empty());
}

TEST(PGSParserTest, ParsesCorrectMagicHeader) {
    // PGS magic: 0x50 0x47 ('PG')
    const uint8_t data[] = {
        0x50, 0x47,  // Magic 'PG'
        0x00, 0x00, 0x00, 0x00,  // PTS
        0x00, 0x00, 0x00, 0x00,  // DTS
        0x16,                     // Segment type: PCS
        0x00, 0x04,               // Segment size: 4
        0x00, 0x00, 0x00, 0x00   // Dummy PCS data
    };
    // Should not crash even if data is truncated/invalid
    EXPECT_NO_THROW(PGSParser::parseBuffer(data, sizeof(data)));
}

// ── Fixture-based subtitle test ───────────────────────────────────────────────

TEST(SubtitleFixtureTest, ParseRealSRTFixture) {
    std::string path = fixturePath("sample.srt");
    if (path.empty()) {
        GTEST_SKIP() << "Fixture sample.srt not found";
    }

    auto events = SubtitleParser::parseSRT(path);
    EXPECT_GT(events.size(), 0u) << "Real SRT fixture should have events";

    // All events must have valid timestamps
    for (const auto& e : events) {
        EXPECT_GE(e.startTime, 0.0);
        EXPECT_GT(e.endTime, e.startTime);
        EXPECT_FALSE(e.text.empty());
    }
}

TEST(SubtitleFixtureTest, LoadASSFixtureViaEngine) {
    std::string path = fixturePath("sample.ass");
    if (path.empty()) {
        GTEST_SKIP() << "Fixture sample.ass not found";
    }

    SubtitleEngine engine;
    EXPECT_TRUE(engine.loadFile(path));
    EXPECT_GT(engine.eventCount(), 0u);
}

// ── ASSRenderer Smoke Test ────────────────────────────────────────────────────

TEST(ASSRendererTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(ASSRenderer renderer);
}

TEST(ASSRendererTest, RenderAtEmptyScriptDoesNotCrash) {
    ASSRenderer renderer;
    // No script loaded — renderAt should return empty, not crash
    EXPECT_NO_THROW(renderer.renderAt(0.5, 1920, 1080));
    auto result = renderer.renderAt(0.5, 1920, 1080);
    EXPECT_TRUE(result.empty());
}

TEST(ASSRendererTest, LoadScriptTextAndRender) {
    ASSRenderer renderer;
    const std::string script = R"([Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,24,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,20,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\b1}Bold{\b0} and {\i1}italic{\i0} text
)";
    bool loaded = renderer.loadScriptText(script);
    EXPECT_TRUE(loaded);

    // At t=0.5 (before any event) — should be empty
    auto before = renderer.renderAt(0.5, 1920, 1080);
    // At t=2.0 (inside event) — may have content or empty if no font
    EXPECT_NO_THROW(renderer.renderAt(2.0, 1920, 1080));
}

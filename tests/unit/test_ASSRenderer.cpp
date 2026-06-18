#include <gtest/gtest.h>
#include "subtitle/ASSRenderer.h"
#include <fstream>
#include <cstdio>

using namespace aurora::subtitle;

static std::string writeTempASS(const std::string& content) {
    std::string path = std::tmpnam(nullptr);
    path += ".ass";
    std::ofstream f(path);
    f << content;
    return path;
}

// ── Style block parsing ───────────────────────────────────────────────────────

TEST(ASSRenderer, ParseStyles_DefaultStyle) {
    std::string script =
        "[Script Info]\n"
        "Title: Test\n\n"
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
        "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
        "Alignment, MarginL, MarginR, MarginV\n"
        "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,"
        "0,0,0,0,100,100,0,0,1,2,2,2,10,10,20\n";

    auto styles = ASSRenderer::parseStyles(script);
    ASSERT_TRUE(styles.count("Default"));
    auto& def = styles["Default"];
    EXPECT_EQ(def.fontName, "Arial");
    EXPECT_EQ(def.fontSize, 20);
    EXPECT_EQ(def.bold, false);
    EXPECT_EQ(def.alignment, 2);
    EXPECT_FLOAT_EQ(def.outline, 2.0f);
}

TEST(ASSRenderer, ParseStyles_BoldItalicFlags) {
    std::string script =
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
        "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
        "Alignment, MarginL, MarginR, MarginV\n"
        "Style: Emphasis,Arial,24,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,"
        "1,1,0,0,100,100,0,0,1,2,2,2,10,10,20\n";

    auto styles = ASSRenderer::parseStyles(script);
    ASSERT_TRUE(styles.count("Emphasis"));
    EXPECT_TRUE(styles["Emphasis"].bold);
    EXPECT_TRUE(styles["Emphasis"].italic);
}

// ── Color parsing (BGR → RGBA) ────────────────────────────────────────────────

TEST(ASSRenderer, ParseColor_PureRed) {
    // ASS color format: &HBBGGRR& → pure red = &H000000FF&... wait, red channel
    // is the LOW byte. &H0000FF& means BB=00 GG=00 RR=FF → red.
    auto color = ASSRenderer::parseStyles(
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
        "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
        "Alignment, MarginL, MarginR, MarginV\n"
        "Style: Red,Arial,20,&H000000FF,&H000000FF,&H00000000,&H80000000,"
        "0,0,0,0,100,100,0,0,1,2,2,2,10,10,20\n"
    )["Red"].primaryColor;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >>  8) & 0xFF;

    EXPECT_GT(r, 200);  // red channel should be dominant
    EXPECT_LT(g, 50);
    EXPECT_LT(b, 50);
}

// ── Events parsing ────────────────────────────────────────────────────────────

TEST(ASSRenderer, ParseEvents_SimpleDialogue) {
    std::string script =
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,Hello World\n";

    auto events = ASSRenderer::parseEvents(script);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_DOUBLE_EQ(events[0].startTime, 1.0);
    EXPECT_DOUBLE_EQ(events[0].endTime, 3.5);
    EXPECT_EQ(events[0].styleName, "Default");
    EXPECT_EQ(events[0].text, "Hello World");
}

TEST(ASSRenderer, ParseEvents_TextWithCommas) {
    std::string script =
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,Hello, World, Test\n";

    auto events = ASSRenderer::parseEvents(script);
    ASSERT_EQ(events.size(), 1u);
    // Text field is the last field — commas inside it must be preserved
    EXPECT_EQ(events[0].text, "Hello, World, Test");
}

TEST(ASSRenderer, ParseEvents_OverrideTagsPreserved) {
    std::string script =
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,{\\b1}Bold{\\b0} text\n";

    auto events = ASSRenderer::parseEvents(script);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_NE(events[0].text.find("\\b1"), std::string::npos);
}

// ── Full script load ──────────────────────────────────────────────────────────

TEST(ASSRenderer, LoadScript_FullFile) {
    std::string content =
        "[Script Info]\n"
        "Title: Aurora Test\n\n"
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
        "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
        "Alignment, MarginL, MarginR, MarginV\n"
        "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,"
        "0,0,0,0,100,100,0,0,1,2,2,2,10,10,20\n\n"
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,Hello World\n"
        "Dialogue: 0,0:00:04.00,0:00:06.00,Default,,0,0,0,,Second Line\n";

    auto path = writeTempASS(content);
    ASSRenderer renderer;
    ASSERT_TRUE(renderer.loadScript(path));
    EXPECT_EQ(renderer.events().size(), 2u);
    EXPECT_TRUE(renderer.styles().count("Default"));
    std::remove(path.c_str());
}

TEST(ASSRenderer, LoadScript_NonexistentFile) {
    ASSRenderer renderer;
    EXPECT_FALSE(renderer.loadScript("/nonexistent/file.ass"));
}

// ── RenderAt timing logic (no font dependency for empty-result cases) ──────────

TEST(ASSRenderer, RenderAt_NoActiveEventsOutsideRange) {
    std::string content =
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:03.50,Default,,0,0,0,,Hello\n";

    auto path = writeTempASS(content);
    ASSRenderer renderer;
    ASSERT_TRUE(renderer.loadScript(path));

    // Timestamp outside the event range — should produce no output
    auto results = renderer.renderAt(10.0, 1920, 1080);
    EXPECT_TRUE(results.empty());
    std::remove(path.c_str());
}

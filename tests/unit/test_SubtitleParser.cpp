#include <gtest/gtest.h>
#include "subtitle/SubtitleEngine.h"
#include <fstream>
#include <cstdio>

using namespace aurora::subtitle;

// Helper: write temp SRT file
static std::string writeTempSRT(const std::string& content) {
    std::string path = std::tmpnam(nullptr);
    path += ".srt";
    std::ofstream f(path);
    f << content;
    return path;
}

TEST(SubtitleParser, ParseSimpleSRT) {
    auto path = writeTempSRT(
        "1\n00:00:01,000 --> 00:00:03,500\nHello World\n\n"
        "2\n00:00:05,000 --> 00:00:07,000\nAurora Player\n\n"
    );
    SubtitleEngine engine;
    ASSERT_TRUE(engine.loadFile(path));
    EXPECT_EQ(engine.eventCount(), 2u);
    auto active = engine.getActiveAt(2.0);
    ASSERT_FALSE(active.empty());
    EXPECT_EQ(active[0]->text, "Hello World");
    std::remove(path.c_str());
}

TEST(SubtitleParser, FormatDetection) {
    EXPECT_EQ(SubtitleParser::detectFormat("sub.srt"), SubtitleFormat::SRT);
    EXPECT_EQ(SubtitleParser::detectFormat("sub.ass"), SubtitleFormat::ASS);
    EXPECT_EQ(SubtitleParser::detectFormat("sub.vtt"), SubtitleFormat::VTT);
    EXPECT_EQ(SubtitleParser::detectFormat("sub.sup"), SubtitleFormat::PGS);
}

TEST(SubtitleEngine, Delay) {
    auto path = writeTempSRT("1\n00:00:01,000 --> 00:00:03,000\nTest\n\n");
    SubtitleEngine engine;
    engine.loadFile(path);
    engine.setDelay(-0.5);   // shift back by 0.5s
    auto at0_5 = engine.getActiveAt(0.5);  // with delay → 0.5-0.5=0 → before start
    EXPECT_TRUE(at0_5.empty());
    auto at1_5 = engine.getActiveAt(1.5);  // 1.5-0.5=1.0 → active
    EXPECT_FALSE(at1_5.empty());
    std::remove(path.c_str());
}

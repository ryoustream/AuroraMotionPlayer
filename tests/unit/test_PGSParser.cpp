#include <gtest/gtest.h>
#include "subtitle/PGSParser.h"
#include <vector>
#include <cstdint>

using namespace aurora::subtitle;

// ── YCbCr → RGBA conversion ───────────────────────────────────────────────────

TEST(PGSParser, YCbCrToRGBA_White) {
    PGSParser::PaletteEntry white;
    white.Y = 235; white.Cb = 128; white.Cr = 128; white.A = 255;

    uint8_t r, g, b, a;
    PGSParser::ycbcrToRGBA(white, r, g, b, a);

    // Near-white luma should map close to RGB (255,255,255)
    EXPECT_GT(r, 200);
    EXPECT_GT(g, 200);
    EXPECT_GT(b, 200);
    EXPECT_EQ(a, 255);
}

TEST(PGSParser, YCbCrToRGBA_Black) {
    PGSParser::PaletteEntry black;
    black.Y = 16; black.Cb = 128; black.Cr = 128; black.A = 255;

    uint8_t r, g, b, a;
    PGSParser::ycbcrToRGBA(black, r, g, b, a);

    EXPECT_LT(r, 30);
    EXPECT_LT(g, 30);
    EXPECT_LT(b, 30);
}

TEST(PGSParser, YCbCrToRGBA_TransparentAlpha) {
    PGSParser::PaletteEntry transparent;
    transparent.A = 0;

    uint8_t r, g, b, a;
    PGSParser::ycbcrToRGBA(transparent, r, g, b, a);
    EXPECT_EQ(a, 0);
}

// ── RLE decoding ───────────────────────────────────────────────────────────────

TEST(PGSParser, DecodeRLE_SinglePixelRuns) {
    PGSParser::Palette pal;
    pal.entries[1] = {235, 128, 128, 255}; // white-ish
    pal.entries[2] = {16,  128, 128, 255}; // black-ish

    // 2x1 image: pixel idx=1, pixel idx=2 (single-pixel encoding)
    std::vector<uint8_t> rle = { 0x01, 0x02 };

    std::vector<uint8_t> rgba;
    ASSERT_TRUE(PGSParser::decodeRLE(rle, 2, 1, pal, rgba));
    ASSERT_EQ(rgba.size(), static_cast<size_t>(2 * 1 * 4));

    // First pixel (idx=1) should be brighter than second pixel (idx=2)
    EXPECT_GT(rgba[0], rgba[4]); // R channel: pixel0 > pixel1
}

TEST(PGSParser, DecodeRLE_TransparentRun) {
    PGSParser::Palette pal;
    pal.entries[0] = {16, 128, 128, 0}; // index 0 = transparent

    // 4 transparent pixels: 0x00 0x04 (short transparent run)
    std::vector<uint8_t> rle = { 0x00, 0x04 };

    std::vector<uint8_t> rgba;
    ASSERT_TRUE(PGSParser::decodeRLE(rle, 4, 1, pal, rgba));
    ASSERT_EQ(rgba.size(), static_cast<size_t>(4 * 1 * 4));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(rgba[i * 4 + 3], 0); // all alpha = 0
    }
}

TEST(PGSParser, DecodeRLE_ColoredRun) {
    PGSParser::Palette pal;
    pal.entries[5] = {200, 128, 128, 255};

    // Run of 3 pixels with palette index 5: 0x00 0xC5 (0x80|0x40, count=5&0x3F=5... )
    // Use the (0x80-0xC0) encoding: b1 in [0x80,0xC0) => count = b1 & 0x3F, color = next byte
    // b1 = 0x83 → count = 3, then color byte = 5
    std::vector<uint8_t> rle = { 0x00, 0x83, 0x05 };

    std::vector<uint8_t> rgba;
    ASSERT_TRUE(PGSParser::decodeRLE(rle, 3, 1, pal, rgba));
    ASSERT_EQ(rgba.size(), static_cast<size_t>(3 * 1 * 4));

    // All 3 pixels should be identical (same palette entry)
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(rgba[i * 4 + 3], 255);
    }
}

TEST(PGSParser, DecodeRLE_EndOfLine) {
    PGSParser::Palette pal;
    pal.entries[1] = {235, 128, 128, 255};

    // Row 1: single pixel idx=1, then end-of-line marker (0x00 0x00)
    // Row 2: single pixel idx=1
    std::vector<uint8_t> rle = { 0x01, 0x00, 0x00, 0x01 };

    std::vector<uint8_t> rgba;
    ASSERT_TRUE(PGSParser::decodeRLE(rle, 1, 2, pal, rgba));
    ASSERT_EQ(rgba.size(), static_cast<size_t>(1 * 2 * 4));

    // Both rows should have opaque pixels
    EXPECT_EQ(rgba[3], 255);  // row0 alpha
    EXPECT_EQ(rgba[7], 255);  // row1 alpha
}

// ── File-level parsing (empty / malformed input) ────────────────────────────────

TEST(PGSParser, ParseBuffer_EmptyInput) {
    auto events = PGSParser::parseBuffer(nullptr, 0);
    EXPECT_TRUE(events.empty());
}

TEST(PGSParser, ParseBuffer_InvalidMagic) {
    std::vector<uint8_t> garbage = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    auto events = PGSParser::parseBuffer(garbage.data(), garbage.size());
    EXPECT_TRUE(events.empty());
}

TEST(PGSParser, ParseFile_NonexistentPath) {
    auto events = PGSParser::parseFile("/nonexistent/path/to/file.sup");
    EXPECT_TRUE(events.empty());
}

#include <gtest/gtest.h>
#include "video/VideoFrame.h"
using namespace aurora::video;

TEST(VideoFrame, ConstructionYUV420P) {
    VideoFrame f(1920, 1080, PixelFormat::YUV420P);
    EXPECT_EQ(f.width(),  1920);
    EXPECT_EQ(f.height(), 1080);
    EXPECT_EQ(f.format(), PixelFormat::YUV420P);
    EXPECT_NE(f.data(0), nullptr);  // Y
    EXPECT_NE(f.data(1), nullptr);  // Cb
    EXPECT_NE(f.data(2), nullptr);  // Cr
    EXPECT_EQ(f.linesize(0), 1920);
    EXPECT_EQ(f.linesize(1), 960);
}

TEST(VideoFrame, ConstructionRGBA) {
    VideoFrame f(640, 480, PixelFormat::RGBA);
    EXPECT_EQ(f.linesize(0), 640 * 4);
    EXPECT_NE(f.data(0), nullptr);
    EXPECT_EQ(f.data(1), nullptr); // No second plane
}

TEST(VideoFrame, TimestampCalculation) {
    VideoFrame f(1280, 720, PixelFormat::YUV420P);
    f.setPts(12000);
    f.setTimeBase(1.0 / 12000.0);
    EXPECT_NEAR(f.timestampSeconds(), 1.0, 1e-6);
}

TEST(VideoFrame, MoveSemantics) {
    VideoFrame a(1920, 1080, PixelFormat::YUV420P);
    uint8_t* dataPtr = a.data(0);
    VideoFrame b(std::move(a));
    EXPECT_EQ(b.data(0), dataPtr);
    EXPECT_EQ(b.width(), 1920);
}

TEST(VideoFrame, HDR10Metadata) {
    VideoFrame f(3840, 2160, PixelFormat::YUV420P10LE);
    ColorMetadata meta;
    meta.isHDR  = true;
    meta.transfer = TransferFunction::SMPTE2084;
    meta.masterMaxLum = 1000.0f;
    f.setColorMeta(meta);
    EXPECT_TRUE(f.colorMeta().isHDR);
    EXPECT_NEAR(f.colorMeta().masterMaxLum, 1000.0f, 0.01f);
}

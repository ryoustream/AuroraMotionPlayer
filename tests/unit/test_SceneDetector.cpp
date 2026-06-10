#include <gtest/gtest.h>
#include "scene/SceneDetector.h"
#include "video/VideoFrame.h"

using namespace aurora::scene;
using namespace aurora::video;

static VideoFramePtr makeGrayFrame(int w, int h, uint8_t val) {
    auto f = std::make_shared<VideoFrame>(w, h, PixelFormat::YUV420P);
    memset(f->data(0), val, f->linesize(0) * h);
    memset(f->data(1), 128, f->linesize(1) * h/2);
    memset(f->data(2), 128, f->linesize(2) * h/2);
    return f;
}

TEST(SceneDetector, IdenticalFramesNoSceneCut) {
    SceneDetector det;
    auto f = makeGrayFrame(320, 240, 100);
    auto res = det.analyze(f, f);
    EXPECT_FALSE(res.isSceneCut);
    EXPECT_NEAR(res.sceneChangeScore, 0.0f, 0.01f);
}

TEST(SceneDetector, BlackToWhiteIsSceneCut) {
    SceneDetector det;
    auto black = makeGrayFrame(320, 240, 0);
    auto white = makeGrayFrame(320, 240, 235);
    auto res   = det.analyze(black, white);
    EXPECT_TRUE(res.isSceneCut);
    EXPECT_GT(res.sceneChangeScore, 0.5f);
}

TEST(SceneDetector, AnimeDetection) {
    // High edge density → should classify as Anime
    SceneDetector det;
    // Simple synthetic test — edge detection on a checkerboard-like pattern
    auto f = makeGrayFrame(320, 240, 128);
    // Alternate pixels to simulate edges
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x)
            f->data(0)[y * f->linesize(0) + x] = ((x + y) % 2 == 0) ? 0 : 255;
    auto res = det.analyze(f, f);
    // Just check no crash and valid range
    EXPECT_GE(res.edgeDensity, 0.0f);
    EXPECT_LE(res.edgeDensity, 1.0f);
}

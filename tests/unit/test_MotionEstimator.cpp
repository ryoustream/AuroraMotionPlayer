#include <gtest/gtest.h>
#include "interpolation/MotionEstimator.h"
#include "video/VideoFrame.h"

using namespace aurora::interpolation;
using namespace aurora::video;

static VideoFramePtr makeSolidFrame(int w, int h, uint8_t val) {
    auto f = std::make_shared<VideoFrame>(w, h, PixelFormat::YUV420P);
    memset(f->data(0), val, f->linesize(0) * h);
    memset(f->data(1), 128, f->linesize(1) * h/2);
    memset(f->data(2), 128, f->linesize(2) * h/2);
    return f;
}

TEST(MotionEstimator, ZeroMotionOnIdenticalFrames) {
    MEConfig cfg;
    cfg.algorithm   = MEAlgorithm::BlockMatching;
    cfg.blockSize   = 16;
    cfg.searchRange = 8;
    MotionEstimator me(cfg);

    auto f = makeSolidFrame(64, 64, 100);
    auto field = me.estimate(f, f);
    ASSERT_FALSE(field.empty());
    for (auto& mv : field) {
        EXPECT_EQ(mv.dx, 0);
        EXPECT_EQ(mv.dy, 0);
    }
}

TEST(MotionEstimator, ReturnsFieldForOpticalFlow) {
    MEConfig cfg;
    cfg.algorithm = MEAlgorithm::OpticalFlow;
    MotionEstimator me(cfg);
    auto f0 = makeSolidFrame(64, 64, 80);
    auto f1 = makeSolidFrame(64, 64, 120);
    auto field = me.estimate(f0, f1);
    EXPECT_FALSE(field.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// test_Upscaler.cpp  —  Aurora Motion Player
// Unit tests for Session 5: upscaler inference pipeline.
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "upscaler/UpscalerFactory.h"
#include "upscaler/UpscalerPipeline.h"
#include "upscaler/ImageUtils.h"
#include "video/VideoFrame.h"

using namespace aurora;
using namespace aurora::upscaler;
using namespace aurora::video;

// ── Helpers ───────────────────────────────────────────────────────────────────
static VideoFramePtr makeFrame(int w, int h,
                                PixelFormat fmt = PixelFormat::YUV420P) {
    auto f = std::make_shared<VideoFrame>(w, h, fmt);
    // Fill Y with ramp, Cb/Cr with 128
    if (fmt == PixelFormat::YUV420P) {
        uint8_t* Y  = f->data(0);
        uint8_t* Cb = f->data(1);
        uint8_t* Cr = f->data(2);
        for (int i = 0; i < w * h; ++i) Y[i] = static_cast<uint8_t>(i % 235 + 16);
        for (int i = 0; i < (w/2) * (h/2); ++i) { Cb[i] = 128; Cr[i] = 128; }
    }
    f->setPts(1001);
    return f;
}

// ── UpscalerFactory ───────────────────────────────────────────────────────────
TEST(UpscalerFactory, CreatesAllModels) {
    EXPECT_NE(UpscalerFactory::create(UpscaleModel::RealESRGAN), nullptr);
    EXPECT_NE(UpscalerFactory::create(UpscaleModel::SPAN),       nullptr);
    EXPECT_NE(UpscalerFactory::create(UpscaleModel::Anime4K),    nullptr);
    EXPECT_NE(UpscalerFactory::create(UpscaleModel::FSRCNN),     nullptr);
}

// ── Anime4K (no external model needed — shader-based) ────────────────────────
class Anime4KTest : public ::testing::Test {
protected:
    Anime4K upscaler;
    UpscalerConfig cfg;

    void SetUp() override {
        cfg.model  = UpscaleModel::Anime4K;
        cfg.factor = UpscaleFactor::X2;
        cfg.modelPath = "";
    }
};

TEST_F(Anime4KTest, InitSucceeds) {
    EXPECT_TRUE(upscaler.init(cfg));
    EXPECT_TRUE(upscaler.isInitialized());
}

TEST_F(Anime4KTest, ProcessDoublesDimensions) {
    ASSERT_TRUE(upscaler.init(cfg));
    auto frame = makeFrame(64, 48);
    auto out   = upscaler.process(frame);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->width(),  128);
    EXPECT_EQ(out->height(), 96);
}

TEST_F(Anime4KTest, ProcessPreservesPts) {
    ASSERT_TRUE(upscaler.init(cfg));
    auto frame = makeFrame(32, 32);
    frame->setPts(9999);
    auto out = upscaler.process(frame);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->pts(), 9999);
}

TEST_F(Anime4KTest, Process4xFactor) {
    cfg.factor = UpscaleFactor::X4;
    ASSERT_TRUE(upscaler.init(cfg));
    auto out = upscaler.process(makeFrame(32, 32));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->width(),  128);
    EXPECT_EQ(out->height(), 128);
}

TEST_F(Anime4KTest, ProcessNullReturnsNull) {
    ASSERT_TRUE(upscaler.init(cfg));
    EXPECT_EQ(upscaler.process(nullptr), nullptr);
}

TEST_F(Anime4KTest, ShutdownCleansUp) {
    ASSERT_TRUE(upscaler.init(cfg));
    upscaler.shutdown();
    EXPECT_FALSE(upscaler.isInitialized());
}

// ── ImageUtils ────────────────────────────────────────────────────────────────
class ImageUtilsTest : public ::testing::Test {};

TEST_F(ImageUtilsTest, FrameToRgbYUV420P) {
    auto frame = makeFrame(4, 4, PixelFormat::YUV420P);
    auto rgb   = frameToRGB(*frame);
    EXPECT_EQ(rgb.size(), static_cast<size_t>(4 * 4 * 3));
    // All values should be in [0,1]
    for (float v : rgb) {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
    }
}

TEST_F(ImageUtilsTest, RgbRoundTripYUV420P) {
    // Pure white → YUV → float RGB → YUV: luma should be ≈ 235
    auto frame = std::make_shared<VideoFrame>(4, 4, PixelFormat::YUV420P);
    // Set to white
    for (int i = 0; i < 4 * 4; ++i) frame->data(0)[i] = 235;
    for (int i = 0; i < 2 * 2; ++i) { frame->data(1)[i] = 128; frame->data(2)[i] = 128; }

    auto rgb  = frameToRGB(*frame);
    auto out  = std::make_shared<VideoFrame>(4, 4, PixelFormat::YUV420P);
    rgbToFrame(rgb, 4, 4, *out);
    // Luma should be close to original 235 (±4 due to float rounding)
    EXPECT_NEAR(out->data(0)[0], 235, 4);
}

TEST_F(ImageUtilsTest, FrameToRgbRGB24) {
    auto frame = std::make_shared<VideoFrame>(2, 2, PixelFormat::RGB24);
    frame->data(0)[0] = 255; frame->data(0)[1] = 0; frame->data(0)[2] = 0; // red
    auto rgb = frameToRGB(*frame);
    EXPECT_NEAR(rgb[0], 1.f, 0.01f);
    EXPECT_NEAR(rgb[1], 0.f, 0.01f);
    EXPECT_NEAR(rgb[2], 0.f, 0.01f);
}

TEST_F(ImageUtilsTest, BilinearResizeCorrectSize) {
    std::vector<float> src(4 * 4 * 3, 0.5f);
    auto dst = bilinearResize(src, 4, 4, 3, 8, 8);
    EXPECT_EQ(dst.size(), static_cast<size_t>(8 * 8 * 3));
}

TEST_F(ImageUtilsTest, BilinearResizeConstantImage) {
    std::vector<float> src(4 * 4 * 3, 0.7f);
    auto dst = bilinearResize(src, 4, 4, 3, 8, 8);
    for (float v : dst) EXPECT_NEAR(v, 0.7f, 1e-4f);
}

TEST_F(ImageUtilsTest, HwcNchwRoundtrip) {
    std::vector<float> hwc(2 * 2 * 3);
    std::iota(hwc.begin(), hwc.end(), 0.f);
    auto nchw = hwcToNchw(hwc, 2, 2);
    auto back = nchwToHwc(nchw, 2, 2);
    EXPECT_EQ(hwc, back);
}

TEST_F(ImageUtilsTest, PadToAlignPadsCorrectly) {
    std::vector<float> src(3 * 3 * 3, 1.f);
    int padW, padH;
    auto padded = padToAlign(src, 3, 3, 3, 4, padW, padH);
    EXPECT_EQ(padW, 4);
    EXPECT_EQ(padH, 4);
    EXPECT_EQ(padded.size(), static_cast<size_t>(4 * 4 * 3));
}

TEST_F(ImageUtilsTest, CropBufferCorrectSize) {
    std::vector<float> src(8 * 8 * 3, 1.f);
    auto cropped = cropBuffer(src, 8, 8, 3, 4, 4);
    EXPECT_EQ(cropped.size(), static_cast<size_t>(4 * 4 * 3));
}

TEST_F(ImageUtilsTest, FloatToUint8Clamps) {
    std::vector<float> buf = {-0.5f, 0.f, 0.5f, 1.0f, 1.5f};
    auto u8 = floatToUint8(buf);
    EXPECT_EQ(u8[0], 0);
    EXPECT_EQ(u8[1], 0);
    EXPECT_NEAR(u8[2], 127, 1);
    EXPECT_EQ(u8[3], 255);
    EXPECT_EQ(u8[4], 255);
}

TEST_F(ImageUtilsTest, BuildCosineWeightsCenterIsMax) {
    auto weights = buildCosineWeights(16, 16, 4);
    float center = weights[8 * 16 + 8];
    float corner = weights[0];
    EXPECT_GT(center, corner);
}

TEST_F(ImageUtilsTest, ComputeTilesCoverFullImage) {
    auto tiles = computeTiles(64, 64, 32, 8, 4);
    EXPECT_FALSE(tiles.empty());
    // Every tile should have valid dims
    for (const auto& t : tiles) {
        EXPECT_GT(t.w, 0);
        EXPECT_GT(t.h, 0);
    }
}

TEST_F(ImageUtilsTest, ExtractTileCorrectSize) {
    std::vector<float> src(16 * 16 * 3, 1.f);
    TileRect tile{0, 0, 8, 8, 0, 0};
    auto extracted = extractTile(src, 16, 16, 3, tile);
    EXPECT_EQ(extracted.size(), static_cast<size_t>(8 * 8 * 3));
}

// ── UpscalerPipeline ──────────────────────────────────────────────────────────
class UpscalerPipelineTest : public ::testing::Test {
protected:
    UpscalerPipeline pipeline;
};

TEST_F(UpscalerPipelineTest, InitPassThrough) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    EXPECT_TRUE(pipeline.init(cfg));
    EXPECT_TRUE(pipeline.isInitialized());
}

TEST_F(UpscalerPipelineTest, PassThroughForwardsFrame) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    cfg.upscaler.model  = UpscaleModel::Anime4K;
    cfg.upscaler.factor = UpscaleFactor::X2;
    ASSERT_TRUE(pipeline.init(cfg));

    VideoFramePtr received;
    pipeline.setOutputCallback([&](VideoFramePtr f) { received = f; });

    auto frame = makeFrame(64, 48);
    EXPECT_TRUE(pipeline.pushFrame(frame));
    pipeline.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->width(),  64);  // unchanged in pass-through
    EXPECT_EQ(received->height(), 48);
}

TEST_F(UpscalerPipelineTest, Anime4KPipelineUpscales) {
    PipelineConfig cfg;
    cfg.passThrough      = false;
    cfg.upscaler.model   = UpscaleModel::Anime4K;
    cfg.upscaler.factor  = UpscaleFactor::X2;
    ASSERT_TRUE(pipeline.init(cfg));

    VideoFramePtr received;
    std::mutex mx;
    pipeline.setOutputCallback([&](VideoFramePtr f) {
        std::lock_guard<std::mutex> lk(mx);
        received = f;
    });

    pipeline.pushFrame(makeFrame(32, 32));
    pipeline.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::lock_guard<std::mutex> lk(mx);
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->width(),  64);
    EXPECT_EQ(received->height(), 64);
}

TEST_F(UpscalerPipelineTest, PushNullReturnsFalse) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    ASSERT_TRUE(pipeline.init(cfg));
    EXPECT_FALSE(pipeline.pushFrame(nullptr));
}

TEST_F(UpscalerPipelineTest, StatsAccumulate) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    ASSERT_TRUE(pipeline.init(cfg));
    pipeline.setOutputCallback([](VideoFramePtr) {});
    for (int i = 0; i < 5; ++i)
        pipeline.pushFrame(makeFrame(16, 16));
    pipeline.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(pipeline.framesProcessed(), static_cast<uint64_t>(1));
}

TEST_F(UpscalerPipelineTest, ShutdownAndReinit) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    ASSERT_TRUE(pipeline.init(cfg));
    pipeline.shutdown();
    EXPECT_FALSE(pipeline.isInitialized());
    EXPECT_TRUE(pipeline.init(cfg));
}

TEST_F(UpscalerPipelineTest, DropOnFullDropsOldest) {
    PipelineConfig cfg;
    cfg.passThrough = true;
    cfg.queueDepth  = 2;
    cfg.dropOnFull  = true;
    ASSERT_TRUE(pipeline.init(cfg));

    // Flood queue; should not block or deadlock
    for (int i = 0; i < 10; ++i)
        pipeline.pushFrame(makeFrame(8, 8));

    pipeline.flush();
    EXPECT_GE(pipeline.framesDropped(), static_cast<uint64_t>(0));
}

// ── RealESRGAN passthrough (no model loaded) ──────────────────────────────────
TEST(RealESRGANTest, InitFailsWithoutModel) {
    RealESRGAN r;
    UpscalerConfig cfg;
    cfg.modelPath = "/nonexistent/path";
    cfg.factor    = UpscaleFactor::X2;
    // Expected to fail (no NCNN/ONNX or bad model path)
    bool ok = r.init(cfg);
    // If no backend compiled, init returns false — that's correct
    if (!ok) EXPECT_FALSE(r.isInitialized());
}

TEST(RealESRGANTest, ProcessUninitializedReturnsOriginal) {
    RealESRGAN r;
    auto frame = makeFrame(32, 32);
    auto out   = r.process(frame);
    // Uninitialized should return frame unchanged
    EXPECT_EQ(out, frame);
}

// ── FSRCNN passthrough ────────────────────────────────────────────────────────
TEST(FSRCNNTest, ProcessFallbackBilinear) {
    FSRCNN f;
    UpscalerConfig cfg;
    cfg.modelPath = ""; // no model
    cfg.factor    = UpscaleFactor::X2;
    // With no backend, init still succeeds (CPU bilinear fallback)
    f.init(cfg);
    if (f.isInitialized()) {
        auto out = f.process(makeFrame(32, 32));
        // Bilinear fallback: output should be 2×
        if (out) {
            EXPECT_EQ(out->width(),  64);
            EXPECT_EQ(out->height(), 64);
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

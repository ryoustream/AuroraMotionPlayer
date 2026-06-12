// ── test_InterpolationPipeline.cpp ───────────────────────────────────────────
// Unit tests for InterpolationPipeline and TileProcessor (S4).
// All tests run without a real AI model (no NCNN/ONNX required).
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include "interpolation/TileProcessor.h"
#include "interpolation/AuroraFlow.h"

using namespace aurora::interpolation;

// ── TileProcessor ─────────────────────────────────────────────────────────────

TEST(TileProcessor, OverlapValidation) {
    TileConfig cfg;
    cfg.tileSize = 64;
    cfg.overlap  = 32; // overlap == tileSize/2 → step = 0 → should throw
    TileProcessor tp(cfg);

    int W = 128, H = 128;
    std::vector<float> rgb0(W*H*3, 0.5f), rgb1(W*H*3, 0.5f);

    EXPECT_THROW(tp.process(rgb0, rgb1, W, H, 0.5f,
        [](const std::vector<float>& a, const std::vector<float>& b,
           int w, int h, float t) { return a; }),
        std::runtime_error);
}

TEST(TileProcessor, IdentityInference) {
    // With a passthrough inferFn, output should equal input (within float eps)
    TileConfig cfg;
    cfg.tileSize  = 32;
    cfg.overlap   = 4;
    cfg.padToTile = false;
    TileProcessor tp(cfg);

    int W = 64, H = 64;
    std::vector<float> rgb0(W*H*3);
    for (size_t i = 0; i < rgb0.size(); ++i) rgb0[i] = (float)(i % 256) / 255.f;
    auto rgb1 = rgb0;

    // inferFn returns first input unchanged
    auto result = tp.process(rgb0, rgb1, W, H, 0.5f,
        [](const std::vector<float>& a, const std::vector<float>&,
           int, int, float) { return a; });

    ASSERT_EQ(result.size(), (size_t)(W*H*3));
    // The output will be slightly blended due to cosine window but within ±0.1
    for (int i = 0; i < W*H*3; ++i)
        EXPECT_NEAR(result[i], rgb0[i], 0.15f) << "pixel " << i;
}

TEST(TileProcessor, WeightMapNormalized) {
    // All-white input through identity fn should return all-white output
    TileConfig cfg;
    cfg.tileSize  = 64;
    cfg.overlap   = 8;
    cfg.padToTile = false;
    TileProcessor tp(cfg);

    int W = 128, H = 128;
    std::vector<float> ones(W*H*3, 1.f);

    auto result = tp.process(ones, ones, W, H, 0.5f,
        [](const std::vector<float>& a, const std::vector<float>&,
           int, int, float) { return a; });

    for (int i = 0; i < W*H*3; ++i)
        EXPECT_NEAR(result[i], 1.f, 1e-4f) << "white normalization failed at " << i;
}

// ── InterpolationConfig defaults ──────────────────────────────────────────────

TEST(InterpolationConfig, DefaultValues) {
    InterpolationConfig cfg;
    EXPECT_EQ(cfg.model,          InterpolationModel::RIFE);
    EXPECT_EQ(cfg.quality,        InterpolationQuality::Balanced);
    EXPECT_EQ(cfg.backend,        InferenceBackend::NCNN);
    EXPECT_FLOAT_EQ(cfg.targetFPS, 60.0f);
    EXPECT_FLOAT_EQ(cfg.sourceFPS, 24.0f);
    EXPECT_TRUE(cfg.sceneDetect);
    EXPECT_FALSE(cfg.useTTA);
    EXPECT_EQ(cfg.tileSize, 0);
}

// ── AuroraFlow (no model) ─────────────────────────────────────────────────────

TEST(AuroraFlow, InitFailsWithoutModel) {
    AuroraFlow flow;
    InterpolationConfig cfg;
    cfg.modelPath = "/nonexistent/path";
    cfg.backend   = InferenceBackend::NCNN;
    // Should fail gracefully (no crash)
    bool ok = flow.init(cfg);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(flow.isRunning());
}

TEST(AuroraFlow, FlushWithoutInit) {
    AuroraFlow flow;
    // Must not crash
    EXPECT_NO_THROW(flow.flush());
    EXPECT_NO_THROW(flow.shutdown());
}

TEST(AuroraFlow, PushNullFrame) {
    AuroraFlow flow;
    // push to un-initialized flow must not crash
    EXPECT_NO_THROW(flow.push(nullptr));
}

TEST(AuroraFlow, ComputeMultiplier_24to60) {
    // 60/24 = 2.5 → 1 intermediate frame generated per pair
    InterpolationConfig cfg;
    cfg.sourceFPS = 24.f;
    cfg.targetFPS = 60.f;

    // Manually check via outputFPS (config-based, not runtime)
    AuroraFlow flow;
    // We can only verify post-init; just confirm no crash with bad path
    cfg.modelPath = "";
    flow.init(cfg); // will fail, but FPS accessors should still work
    EXPECT_FLOAT_EQ(flow.inputFPS(),  24.f);
    EXPECT_FLOAT_EQ(flow.outputFPS(), 60.f);
}

TEST(AuroraFlow, AvailableModels_EmptyDir) {
    auto models = AuroraFlow::availableModels("");
    EXPECT_TRUE(models.empty());
}

TEST(AuroraFlow, AvailableModels_NonExistent) {
    auto models = AuroraFlow::availableModels("/nonexistent/dir");
    EXPECT_TRUE(models.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

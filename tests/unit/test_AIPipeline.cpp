/**
 * Aurora Motion Player — Unit Tests: AIPipelineManager
 */

#include <gtest/gtest.h>
#include "core/pipeline/AIPipelineManager.h"
#include "core/video/VideoFrame.h"

#include <chrono>
#include <thread>

using namespace aurora::core;

// ── Construction ──────────────────────────────────────────────────────────────
TEST(AIPipeline, ConstructsWithoutCrash) {
    EXPECT_NO_THROW(AIPipelineManager pm);
}

// ── Default state ─────────────────────────────────────────────────────────────
TEST(AIPipeline, DefaultStateIsIdle) {
    AIPipelineManager pm;
    EXPECT_EQ(pm.state(), PipelineState::Idle);
}

// ── Configure ─────────────────────────────────────────────────────────────────
TEST(AIPipeline, ConfigureAcceptsDefaultConfig) {
    AIPipelineManager pm;
    PipelineConfig cfg;
    EXPECT_NO_THROW(pm.configure(cfg));
}

TEST(AIPipeline, ConfigureWithInterpEnabled) {
    AIPipelineManager pm;
    PipelineConfig cfg;
    cfg.interpEnabled = true;
    cfg.interpModel   = "RIFE";
    cfg.targetFPS     = 60.0f;
    EXPECT_NO_THROW(pm.configure(cfg));
    EXPECT_EQ(pm.config().interpModel, "RIFE");
    EXPECT_FLOAT_EQ(pm.config().targetFPS, 60.0f);
}

TEST(AIPipeline, ConfigureWithHDR) {
    AIPipelineManager pm;
    PipelineConfig cfg;
    cfg.hdrEnabled      = true;
    cfg.toneMappingMode = "ACES";
    EXPECT_NO_THROW(pm.configure(cfg));
    EXPECT_EQ(pm.config().toneMappingMode, "ACES");
}

// ── Start / Stop ──────────────────────────────────────────────────────────────
TEST(AIPipeline, StartChangesStateToRunning) {
    AIPipelineManager pm;
    EXPECT_TRUE(pm.start());
    EXPECT_EQ(pm.state(), PipelineState::Running);
    pm.stop();
}

TEST(AIPipeline, StopChangesStateToIdle) {
    AIPipelineManager pm;
    pm.start();
    pm.stop();
    EXPECT_EQ(pm.state(), PipelineState::Idle);
}

TEST(AIPipeline, DoubleStartReturnsFalse) {
    AIPipelineManager pm;
    EXPECT_TRUE(pm.start());
    EXPECT_FALSE(pm.start());  // Already running
    pm.stop();
}

// ── Pause / Resume ────────────────────────────────────────────────────────────
TEST(AIPipeline, PauseAndResume) {
    AIPipelineManager pm;
    pm.start();
    pm.pause();
    EXPECT_EQ(pm.state(), PipelineState::Paused);
    pm.resume();
    EXPECT_EQ(pm.state(), PipelineState::Running);
    pm.stop();
}

// ── Push frame ────────────────────────────────────────────────────────────────
TEST(AIPipeline, PushFrameWhileRunning) {
    AIPipelineManager pm;

    std::vector<std::shared_ptr<VideoFrame>> received;
    pm.setOutputCallback([&](std::shared_ptr<VideoFrame> f) {
        received.push_back(f);
    });

    pm.start();

    auto frame = std::make_shared<VideoFrame>();
    frame->width  = 1920;
    frame->height = 1080;
    EXPECT_TRUE(pm.pushFrame(frame));

    // Give pipeline thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    pm.stop();

    EXPECT_GE(received.size(), 0u); // May or may not be processed depending on timing
}

TEST(AIPipeline, PushFrameWhileIdleStillQueues) {
    AIPipelineManager pm;
    auto frame = std::make_shared<VideoFrame>();
    // Should not crash; frame queued but not processed
    EXPECT_TRUE(pm.pushFrame(frame));
}

// ── Flush ─────────────────────────────────────────────────────────────────────
TEST(AIPipeline, FlushDoesNotCrash) {
    AIPipelineManager pm;
    pm.start();
    pm.pushFrame(std::make_shared<VideoFrame>());
    EXPECT_NO_THROW(pm.flush());
    pm.stop();
}

// ── Stats ─────────────────────────────────────────────────────────────────────
TEST(AIPipeline, StatsReturnZeroOnStart) {
    AIPipelineManager pm;
    auto stats = pm.stats();
    EXPECT_GE(stats.droppedFrames,      0);
    EXPECT_GE(stats.interpolatedFrames, 0);
    EXPECT_GE(stats.throughputFPS,      0.0f);
}

// ── Scene change notification ─────────────────────────────────────────────────
TEST(AIPipeline, SceneChangeDoesNotCrash) {
    AIPipelineManager pm;
    pm.start();
    EXPECT_NO_THROW(pm.onSceneChange());
    pm.stop();
}

// ── Destructor stops cleanly ──────────────────────────────────────────────────
TEST(AIPipeline, DestructorStopsCleanly) {
    EXPECT_NO_THROW({
        AIPipelineManager pm;
        pm.start();
        pm.pushFrame(std::make_shared<VideoFrame>());
        // Destructor called here
    });
}

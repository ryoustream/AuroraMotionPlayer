/**
 * Aurora Motion Player — Unit Tests: AudioResampler
 */

#include <gtest/gtest.h>
#include "core/audio/AudioResampler.h"

using namespace aurora::core;

// ── Construction ──────────────────────────────────────────────────────────────
TEST(AudioResampler, ConstructsWithoutCrash) {
    EXPECT_NO_THROW(AudioResampler r);
}

TEST(AudioResampler, NotOpenByDefault) {
    AudioResampler r;
    EXPECT_FALSE(r.isOpen());
}

// ── Open / Close ──────────────────────────────────────────────────────────────
TEST(AudioResampler, OpenWithSameFormatSucceeds) {
    AudioResampler r;
    ResamplerConfig cfg;
    cfg.inSampleRate  = 48000;
    cfg.outSampleRate = 48000;
    cfg.inChannels    = 2;
    cfg.outChannels   = 2;
    cfg.inFormat      = AV_SAMPLE_FMT_FLT;
    cfg.outFormat     = AV_SAMPLE_FMT_FLT;

    EXPECT_TRUE(r.open(cfg));
    EXPECT_TRUE(r.isOpen());
    r.close();
    EXPECT_FALSE(r.isOpen());
}

TEST(AudioResampler, OpenWithRateConversionSucceeds) {
    AudioResampler r;
    ResamplerConfig cfg;
    cfg.inSampleRate  = 44100;
    cfg.outSampleRate = 48000;
    cfg.inChannels    = 2;
    cfg.outChannels   = 2;

    EXPECT_TRUE(r.open(cfg));
    EXPECT_TRUE(r.isOpen());
    r.close();
}

TEST(AudioResampler, OpenWithChannelDownmixSucceeds) {
    AudioResampler r;
    ResamplerConfig cfg;
    cfg.inSampleRate  = 48000;
    cfg.outSampleRate = 48000;
    cfg.inChannels    = 6;   // 5.1
    cfg.outChannels   = 2;   // stereo

    EXPECT_TRUE(r.open(cfg));
    r.close();
}

TEST(AudioResampler, CloseIsIdempotent) {
    AudioResampler r;
    r.close();
    r.close(); // Should not crash
}

// ── Config ────────────────────────────────────────────────────────────────────
TEST(AudioResampler, ConfigStoredCorrectly) {
    AudioResampler r;
    ResamplerConfig cfg;
    cfg.inSampleRate  = 44100;
    cfg.outSampleRate = 96000;
    r.open(cfg);
    EXPECT_EQ(r.config().inSampleRate,  44100);
    EXPECT_EQ(r.config().outSampleRate, 96000);
    r.close();
}

// ── Output sample count estimation ───────────────────────────────────────────
TEST(AudioResampler, OutputSampleEstimationPositive) {
    AudioResampler r;
    ResamplerConfig cfg;
    cfg.inSampleRate  = 44100;
    cfg.outSampleRate = 48000;
    r.open(cfg);
    int out = r.outputSamples(1024);
    EXPECT_GT(out, 0);
    EXPECT_LT(out, 10000); // Reasonable upper bound
    r.close();
}

// ── Flush without data doesn't crash ─────────────────────────────────────────
TEST(AudioResampler, FlushReturnsEmptyOrValidData) {
    AudioResampler r;
    ResamplerConfig cfg;
    r.open(cfg);
    auto result = r.flush();
    // May be empty or contain buffered zeros — should not crash
    EXPECT_NO_THROW(r.flush());
    r.close();
}

// ── Speed ─────────────────────────────────────────────────────────────────────
TEST(AudioResampler, DefaultSpeedIsOne) {
    AudioResampler r;
    EXPECT_FLOAT_EQ(r.speed(), 1.0f);
}

TEST(AudioResampler, SetSpeedClampedToMinimum) {
    AudioResampler r;
    r.setSpeed(-5.0f);
    EXPECT_GT(r.speed(), 0.0f);
}

TEST(AudioResampler, SetSpeedClampedToMaximum) {
    AudioResampler r;
    r.setSpeed(99.0f);
    EXPECT_LE(r.speed(), 10.0f);
}

TEST(AudioResampler, SetAndGetSpeed) {
    AudioResampler r;
    r.setSpeed(1.5f);
    EXPECT_FLOAT_EQ(r.speed(), 1.5f);
}

// ── Sync offset ───────────────────────────────────────────────────────────────
TEST(AudioResampler, DefaultSyncOffsetIsZero) {
    AudioResampler r;
    EXPECT_EQ(r.syncOffsetMs(), 0);
}

TEST(AudioResampler, SetSyncOffset) {
    AudioResampler r;
    r.setSyncOffsetMs(-200);
    EXPECT_EQ(r.syncOffsetMs(), -200);
}

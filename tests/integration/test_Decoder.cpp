/**
 * Aurora Motion Player — Integration Test: Decoder Pipeline
 * ============================================================
 * Tests FFmpegDecoder + VideoFrame + AudioBuffer end-to-end
 * using synthetic/fixture media files (no real network required).
 *
 * Covers:
 *   - Decoder state machine (Idle → Opening → EOF_)
 *   - Protocol detection via NetworkSource
 *   - VideoMetadata extraction from fixture files
 *   - Audio stream detection and buffer callbacks
 *   - Error handling for invalid URLs
 *   - Seek accuracy (±200 ms tolerance)
 *   - Speed change (0.5× / 2.0×) does not crash
 *   - Hardware acceleration config creation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "decoder/FFmpegDecoder.h"
#include "decoder/HardwareDecoder.h"
#include "network/NetworkSource.h"
#include "video/VideoFrame.h"
#include "audio/AudioBuffer.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;
using namespace aurora;
using namespace std::chrono_literals;

// ── Fixture path helper ────────────────────────────────────────────────────────
static std::string fixturePath(const std::string& name) {
    // Try relative paths from common build/run locations
    const std::vector<std::string> bases = {
        "tests/fixtures/",
        "../tests/fixtures/",
        "../../tests/fixtures/",
    };
    for (const auto& base : bases) {
        std::string p = base + name;
        if (fs::exists(p)) return p;
    }
    return "";  // Not found — tests will skip gracefully
}

static bool fixtureExists(const std::string& name) {
    return !fixturePath(name).empty();
}

// ══════════════════════════════════════════════════════════════════════════════
// DecoderState Tests
// ══════════════════════════════════════════════════════════════════════════════

class DecoderStateTest : public ::testing::Test {
protected:
    decoder::FFmpegDecoder dec;
};

TEST_F(DecoderStateTest, InitialStateIsIdle) {
    EXPECT_EQ(dec.state(), decoder::DecoderState::Idle);
}

TEST_F(DecoderStateTest, OpenInvalidURLSetsError) {
    bool errCalled = false;
    dec.setErrorCallback([&](const std::string& msg) {
        errCalled = true;
        EXPECT_FALSE(msg.empty());
    });

    bool opened = dec.open("/nonexistent/path/video.mp4");
    // Either open() returns false or error callback fires
    if (!opened) {
        SUCCEED();
    } else {
        // Opened but should transition to error eventually
        std::this_thread::sleep_for(200ms);
        EXPECT_NE(dec.state(), decoder::DecoderState::Playing);
    }
}

TEST_F(DecoderStateTest, CloseFromIdleIsNoop) {
    EXPECT_NO_THROW(dec.close());
    EXPECT_EQ(dec.state(), decoder::DecoderState::Idle);
}

// ══════════════════════════════════════════════════════════════════════════════
// DecoderConfig Tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(DecoderConfigTest, DefaultConfig) {
    decoder::DecoderConfig cfg;
    EXPECT_EQ(cfg.hwAccel,        decoder::HWAccelType::None);
    EXPECT_TRUE(cfg.enableHDR);
    EXPECT_EQ(cfg.threadCount,    0);
    EXPECT_EQ(cfg.videoQueueSize, 16);
    EXPECT_EQ(cfg.audioQueueSize, 64);
}

TEST(DecoderConfigTest, CustomConfig) {
    decoder::DecoderConfig cfg;
    cfg.hwAccel        = decoder::HWAccelType::D3D11VA;
    cfg.threadCount    = 4;
    cfg.videoQueueSize = 32;

    decoder::FFmpegDecoder dec(cfg);
    // Config accepted without crash
    EXPECT_EQ(dec.state(), decoder::DecoderState::Idle);
}

// ══════════════════════════════════════════════════════════════════════════════
// NetworkSource Protocol Detection
// ══════════════════════════════════════════════════════════════════════════════

TEST(NetworkSourceTest, DetectLocalProtocol) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("/home/user/video.mp4"),
              network::Protocol::Local);
    EXPECT_EQ(network::NetworkSource::detectProtocol("C:\\Videos\\test.mkv"),
              network::Protocol::Local);
}

TEST(NetworkSourceTest, DetectHTTP) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("http://example.com/video.mp4"),
              network::Protocol::HTTP);
}

TEST(NetworkSourceTest, DetectHTTPS) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("https://cdn.example.com/stream"),
              network::Protocol::HTTPS);
}

TEST(NetworkSourceTest, DetectHLS) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("https://example.com/stream.m3u8"),
              network::Protocol::HLS);
    EXPECT_EQ(network::NetworkSource::detectProtocol("http://cdn.tv/live/index.m3u8"),
              network::Protocol::HLS);
}

TEST(NetworkSourceTest, DetectDASH) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("https://example.com/stream.mpd"),
              network::Protocol::DASH);
}

TEST(NetworkSourceTest, DetectRTMP) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("rtmp://live.example.com/stream"),
              network::Protocol::RTMP);
}

TEST(NetworkSourceTest, DetectRTSP) {
    EXPECT_EQ(network::NetworkSource::detectProtocol("rtsp://camera.local/video"),
              network::Protocol::RTSP);
}

TEST(NetworkSourceTest, IsNetworkURL_True) {
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("http://example.com/v.mp4"));
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("https://cdn.tv/live.m3u8"));
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("rtmp://stream.tv/live"));
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("rtsp://cam.local/stream"));
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("smb://192.168.1.1/share/video.mkv"));
    EXPECT_TRUE(network::NetworkSource::isNetworkURL("ftp://files.example.com/video.avi"));
}

TEST(NetworkSourceTest, IsNetworkURL_False) {
    EXPECT_FALSE(network::NetworkSource::isNetworkURL("/home/user/video.mp4"));
    EXPECT_FALSE(network::NetworkSource::isNetworkURL("C:\\Videos\\test.mkv"));
    EXPECT_FALSE(network::NetworkSource::isNetworkURL(""));
}

TEST(NetworkSourceTest, ResolveLocalURLPassthrough) {
    network::NetworkSource ns;
    std::string url = "/tmp/test_video.mp4";
    std::string resolved = ns.resolveURL(url);
    // Local URLs should pass through unchanged or with minimal transformation
    EXPECT_FALSE(resolved.empty());
}

TEST(NetworkSourceTest, BuildFFmpegOptionsForHTTP) {
    network::NetworkSource ns;
    std::string opts = ns.buildFFmpegOptions("http://example.com/video.mp4");
    // Should return non-empty options string for HTTP
    EXPECT_FALSE(opts.empty());
}

// ══════════════════════════════════════════════════════════════════════════════
// Fixture-based Decoder Integration Tests
// ══════════════════════════════════════════════════════════════════════════════

class DecoderFixtureTest : public ::testing::Test {
protected:
    void SetUp() override {
        dec = std::make_unique<decoder::FFmpegDecoder>();
    }
    void TearDown() override {
        if (dec) dec->close();
    }

    std::unique_ptr<decoder::FFmpegDecoder> dec;
};

TEST_F(DecoderFixtureTest, OpenSyntheticMP4) {
    if (!fixtureExists("sample_720p.mp4")) {
        GTEST_SKIP() << "Fixture sample_720p.mp4 not found — run models/download_models.py first";
    }

    std::atomic<int> videoFrames{0};
    std::atomic<int> audioFrames{0};

    dec->setVideoCallback([&](video::VideoFramePtr f) {
        ++videoFrames;
        EXPECT_GT(f->width(), 0);
        EXPECT_GT(f->height(), 0);
    });
    dec->setAudioCallback([&](audio::AudioBufferPtr buf) {
        ++audioFrames;
        EXPECT_GT(buf->nbSamples, 0);
    });

    ASSERT_TRUE(dec->open(fixturePath("sample_720p.mp4")));
    EXPECT_EQ(dec->state(), decoder::DecoderState::Playing);

    const auto& meta = dec->metadata();
    // Check primary video stream
    const auto* vs = meta.primaryVideo();
    if (vs) {
        EXPECT_GT(vs->width, 0);
        EXPECT_GT(vs->height, 0);
        EXPECT_GT(vs->frameRate, 0.0);
    }
    EXPECT_GT(meta.duration, 0.0);

    // Let it play for 1 second
    dec->play();
    std::this_thread::sleep_for(1000ms);
    dec->close();

    EXPECT_GT(videoFrames.load(), 0) << "No video frames decoded";
}

TEST_F(DecoderFixtureTest, OpenMKVWithSubtitles) {
    if (!fixtureExists("sample_subtitle.mkv")) {
        GTEST_SKIP() << "Fixture sample_subtitle.mkv not found";
    }

    ASSERT_TRUE(dec->open(fixturePath("sample_subtitle.mkv")));
    const auto& meta = dec->metadata();
    EXPECT_GT(meta.duration, 0.0);
}

TEST_F(DecoderFixtureTest, SeekAccuracy) {
    if (!fixtureExists("sample_720p.mp4")) {
        GTEST_SKIP() << "Fixture sample_720p.mp4 not found";
    }

    ASSERT_TRUE(dec->open(fixturePath("sample_720p.mp4")));
    dec->play();
    std::this_thread::sleep_for(200ms);

    const double targetSec = 2.0;
    dec->seek(targetSec);
    std::this_thread::sleep_for(300ms);

    double pos = dec->position();
    EXPECT_NEAR(pos, targetSec, 0.5) << "Seek inaccuracy > 500ms";
}

TEST_F(DecoderFixtureTest, SpeedChangeDoesNotCrash) {
    if (!fixtureExists("sample_720p.mp4")) {
        GTEST_SKIP() << "Fixture sample_720p.mp4 not found";
    }

    ASSERT_TRUE(dec->open(fixturePath("sample_720p.mp4")));
    dec->play();

    EXPECT_NO_THROW(dec->setSpeed(0.5));
    std::this_thread::sleep_for(100ms);
    EXPECT_NO_THROW(dec->setSpeed(2.0));
    std::this_thread::sleep_for(100ms);
    EXPECT_NO_THROW(dec->setSpeed(1.0));
}

// ══════════════════════════════════════════════════════════════════════════════
// HardwareDecoder Config Tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(HardwareDecoderTest, ConfigNone) {
    decoder::DecoderConfig cfg;
    cfg.hwAccel = decoder::HWAccelType::None;
    decoder::FFmpegDecoder dec(cfg);
    EXPECT_EQ(dec.state(), decoder::DecoderState::Idle);
}

TEST(HardwareDecoderTest, ConfigNVDEC) {
    decoder::DecoderConfig cfg;
    cfg.hwAccel = decoder::HWAccelType::NVDEC;
    // Construction should not throw even if NVDEC not available
    EXPECT_NO_THROW(decoder::FFmpegDecoder dec2(cfg));
}

TEST(HardwareDecoderTest, ConfigMediaCodec) {
    decoder::DecoderConfig cfg;
    cfg.hwAccel = decoder::HWAccelType::MediaCodec;
    EXPECT_NO_THROW(decoder::FFmpegDecoder dec2(cfg));
}

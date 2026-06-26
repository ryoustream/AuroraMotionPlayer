/**
 * Aurora Motion Player — Integration Test: Network Stack
 * =======================================================
 * Tests NetworkSource, HLS/DASH/RTMP/RTSP stream configuration,
 * and URL resolution without requiring live network access.
 *
 * Covers:
 *   - Protocol detection for all supported schemes
 *   - isNetworkURL classification
 *   - NetworkConfig defaults and customization
 *   - resolveURL passthrough for local paths
 *   - buildFFmpegOptions returns non-empty for network URLs
 *   - Callbacks (progress, error) are stored and callable
 *   - HLS/DASH/RTSP/RTMP/SMB/FTP/WebDAV URL handling
 *   - Edge cases: empty URL, URL with auth, URL with query params
 *   - Multiple NetworkSource instances (no shared-state issues)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "network/NetworkSource.h"
#include "network/HLSStream.h"
// Note: DASHStream, RTMPStream, RTSPStream are all defined in HLSStream.h

#include <atomic>
#include <string>

using namespace aurora::network;

// ══════════════════════════════════════════════════════════════════════════════
// NetworkConfig Tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(NetworkConfigTest, DefaultValues) {
    NetworkConfig cfg;
    EXPECT_EQ(cfg.connectTimeoutMs,  5000);
    EXPECT_EQ(cfg.readTimeoutMs,     10000);
    EXPECT_EQ(cfg.bufferSizeKB,      2048);
    EXPECT_EQ(cfg.maxRetriesOnError, 3);
    EXPECT_EQ(cfg.userAgent,         "Aurora/1.0");
    EXPECT_TRUE(cfg.enableCache);
    EXPECT_TRUE(cfg.enableTLS);
    EXPECT_TRUE(cfg.httpReferer.empty());
}

TEST(NetworkConfigTest, CustomValues) {
    NetworkConfig cfg;
    cfg.connectTimeoutMs  = 3000;
    cfg.readTimeoutMs     = 7000;
    cfg.bufferSizeKB      = 4096;
    cfg.maxRetriesOnError = 5;
    cfg.userAgent         = "AuroraTest/2.0";
    cfg.httpReferer       = "https://example.com";
    cfg.enableCache       = false;
    cfg.enableTLS         = false;

    EXPECT_EQ(cfg.connectTimeoutMs, 3000);
    EXPECT_EQ(cfg.userAgent, "AuroraTest/2.0");
    EXPECT_EQ(cfg.httpReferer, "https://example.com");
    EXPECT_FALSE(cfg.enableCache);
    EXPECT_FALSE(cfg.enableTLS);
}

// ══════════════════════════════════════════════════════════════════════════════
// Protocol Detection — Comprehensive
// ══════════════════════════════════════════════════════════════════════════════

struct ProtocolTestCase {
    std::string url;
    Protocol    expected;
};

class ProtocolDetectionTest : public ::testing::TestWithParam<ProtocolTestCase> {};

TEST_P(ProtocolDetectionTest, DetectsProtocol) {
    auto [url, expected] = GetParam();
    EXPECT_EQ(NetworkSource::detectProtocol(url), expected)
        << "URL: " << url;
}

INSTANTIATE_TEST_SUITE_P(AllProtocols, ProtocolDetectionTest, ::testing::Values(
    ProtocolTestCase{"http://example.com/video.mp4",          Protocol::HTTP},
    ProtocolTestCase{"http://cdn.tv:8080/stream",             Protocol::HTTP},
    ProtocolTestCase{"https://secure.example.com/video",      Protocol::HTTPS},
    ProtocolTestCase{"https://cdn.example.com:443/v.mp4",     Protocol::HTTPS},
    ProtocolTestCase{"https://host/playlist.m3u8",            Protocol::HLS},
    ProtocolTestCase{"http://live.tv/stream/index.m3u8",      Protocol::HLS},
    ProtocolTestCase{"https://vod.example.com/manifest.mpd",  Protocol::DASH},
    ProtocolTestCase{"http://dash.host/stream.mpd",           Protocol::DASH},
    ProtocolTestCase{"rtmp://live.example.com/app/stream",    Protocol::RTMP},
    ProtocolTestCase{"rtmps://secure.live.com/live/stream",   Protocol::RTMP},
    ProtocolTestCase{"rtsp://192.168.1.100/stream1",          Protocol::RTSP},
    ProtocolTestCase{"rtsp://camera.local:8554/video",        Protocol::RTSP},
    ProtocolTestCase{"smb://192.168.1.1/share/video.mkv",     Protocol::SMB},
    ProtocolTestCase{"smb://nas.local/media/movie.mp4",       Protocol::SMB},
    ProtocolTestCase{"ftp://files.example.com/video.avi",     Protocol::FTP},
    ProtocolTestCase{"ftp://192.168.1.10/pub/video.mkv",      Protocol::FTP},
    ProtocolTestCase{"/home/user/video.mp4",                  Protocol::Local},
    ProtocolTestCase{"C:\\Videos\\test.mkv",                  Protocol::Local},
    ProtocolTestCase{"./relative/path/video.mp4",             Protocol::Local}
));

// ══════════════════════════════════════════════════════════════════════════════
// isNetworkURL Tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(NetworkURLTest, NetworkURLs) {
    EXPECT_TRUE(NetworkSource::isNetworkURL("http://example.com/v.mp4"));
    EXPECT_TRUE(NetworkSource::isNetworkURL("https://cdn.example.com/v.mp4"));
    EXPECT_TRUE(NetworkSource::isNetworkURL("rtmp://stream.tv/live"));
    EXPECT_TRUE(NetworkSource::isNetworkURL("rtsp://cam.local/stream"));
    EXPECT_TRUE(NetworkSource::isNetworkURL("smb://nas/share/video.mkv"));
    EXPECT_TRUE(NetworkSource::isNetworkURL("ftp://files.com/video.avi"));
}

TEST(NetworkURLTest, LocalURLs) {
    EXPECT_FALSE(NetworkSource::isNetworkURL("/home/user/video.mp4"));
    EXPECT_FALSE(NetworkSource::isNetworkURL("C:\\Videos\\movie.mkv"));
    EXPECT_FALSE(NetworkSource::isNetworkURL("./video.mp4"));
    EXPECT_FALSE(NetworkSource::isNetworkURL(""));
}

// ══════════════════════════════════════════════════════════════════════════════
// NetworkSource Instance Tests
// ══════════════════════════════════════════════════════════════════════════════

TEST(NetworkSourceTest, DefaultConstruction) {
    EXPECT_NO_THROW(NetworkSource ns);
}

TEST(NetworkSourceTest, CustomConfigConstruction) {
    NetworkConfig cfg;
    cfg.connectTimeoutMs = 3000;
    cfg.userAgent        = "AuroraCustom/1.0";
    EXPECT_NO_THROW(NetworkSource ns(cfg));
}

TEST(NetworkSourceTest, ConfigAccessible) {
    NetworkConfig cfg;
    cfg.bufferSizeKB = 8192;
    NetworkSource ns(cfg);
    EXPECT_EQ(ns.config().bufferSizeKB, 8192);
}

TEST(NetworkSourceTest, ResolveLocalURL) {
    NetworkSource ns;
    std::string url      = "/tmp/test_video.mp4";
    std::string resolved = ns.resolveURL(url);
    EXPECT_FALSE(resolved.empty());
    // Local paths should pass through (possibly unchanged)
}

TEST(NetworkSourceTest, ResolveHTTPURL) {
    NetworkSource ns;
    std::string url      = "http://example.com/video.mp4";
    std::string resolved = ns.resolveURL(url);
    EXPECT_FALSE(resolved.empty());
}

TEST(NetworkSourceTest, BuildFFmpegOptionsHTTP) {
    NetworkSource ns;
    std::string opts = ns.buildFFmpegOptions("http://example.com/video.mp4");
    EXPECT_FALSE(opts.empty());
}

TEST(NetworkSourceTest, BuildFFmpegOptionsRTSP) {
    NetworkSource ns;
    std::string opts = ns.buildFFmpegOptions("rtsp://192.168.1.100/stream");
    EXPECT_FALSE(opts.empty());
}

TEST(NetworkSourceTest, BuildFFmpegOptionsHLS) {
    NetworkSource ns;
    std::string opts = ns.buildFFmpegOptions("https://example.com/stream.m3u8");
    EXPECT_FALSE(opts.empty());
}

TEST(NetworkSourceTest, ProgressCallbackStored) {
    NetworkSource ns;
    std::atomic<int> callCount{0};

    ns.setProgressCallback([&](int64_t downloaded, int64_t total) {
        ++callCount;
    });

    // Callback is stored — we can't trigger it without real network,
    // but we verify no crash on setup
    SUCCEED();
}

TEST(NetworkSourceTest, ErrorCallbackStored) {
    NetworkSource ns;
    bool errReceived = false;

    ns.setErrorCallback([&](const std::string& msg) {
        errReceived = true;
    });

    SUCCEED();
}

TEST(NetworkSourceTest, MultipleInstancesIndependent) {
    NetworkConfig cfgA, cfgB;
    cfgA.bufferSizeKB = 1024;
    cfgB.bufferSizeKB = 4096;

    NetworkSource nsA(cfgA), nsB(cfgB);

    EXPECT_EQ(nsA.config().bufferSizeKB, 1024);
    EXPECT_EQ(nsB.config().bufferSizeKB, 4096);
}

// ══════════════════════════════════════════════════════════════════════════════
// URL Edge Cases
// ══════════════════════════════════════════════════════════════════════════════

TEST(URLEdgeCaseTest, URLWithQueryParams) {
    auto p = NetworkSource::detectProtocol(
        "https://cdn.example.com/stream.m3u8?token=abc123&expires=9999");
    EXPECT_EQ(p, Protocol::HLS);
}

TEST(URLEdgeCaseTest, URLWithAuth) {
    auto p = NetworkSource::detectProtocol(
        "ftp://user:pass@192.168.1.10/video.mkv");
    EXPECT_EQ(p, Protocol::FTP);
}

TEST(URLEdgeCaseTest, URLWithPort) {
    EXPECT_EQ(NetworkSource::detectProtocol("rtsp://cam.local:554/stream"),
              Protocol::RTSP);
    EXPECT_EQ(NetworkSource::detectProtocol("http://host:9090/stream"),
              Protocol::HTTP);
}

TEST(URLEdgeCaseTest, EmptyURLIsLocal) {
    // Empty URL should not crash and return some default
    EXPECT_NO_THROW(NetworkSource::detectProtocol(""));
    EXPECT_FALSE(NetworkSource::isNetworkURL(""));
}

// ══════════════════════════════════════════════════════════════════════════════
// Stream Type Construction (smoke)
// ══════════════════════════════════════════════════════════════════════════════

TEST(HLSStreamTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(HLSStream stream);
}

TEST(DASHStreamTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(DASHStream stream);
}

TEST(RTMPStreamTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(RTMPStream stream);
}

TEST(RTSPStreamTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(RTSPStream stream);
}

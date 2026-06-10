#include <gtest/gtest.h>
#include "network/NetworkSource.h"
using namespace aurora::network;

TEST(NetworkSource, DetectHTTP)   { EXPECT_EQ(NetworkSource::detectProtocol("http://example.com/v.mp4"),  Protocol::HTTP);  }
TEST(NetworkSource, DetectHTTPS)  { EXPECT_EQ(NetworkSource::detectProtocol("https://example.com/v.mp4"), Protocol::HTTPS); }
TEST(NetworkSource, DetectHLS)    { EXPECT_EQ(NetworkSource::detectProtocol("https://cdn.com/stream.m3u8"), Protocol::HLS); }
TEST(NetworkSource, DetectDASH)   { EXPECT_EQ(NetworkSource::detectProtocol("https://cdn.com/manifest.mpd"), Protocol::DASH); }
TEST(NetworkSource, DetectRTMP)   { EXPECT_EQ(NetworkSource::detectProtocol("rtmp://live.example.com/app/stream"), Protocol::RTMP); }
TEST(NetworkSource, DetectRTSP)   { EXPECT_EQ(NetworkSource::detectProtocol("rtsp://192.168.1.1/stream"), Protocol::RTSP); }
TEST(NetworkSource, DetectLocal)  { EXPECT_EQ(NetworkSource::detectProtocol("/home/user/video.mkv"), Protocol::Local); }
TEST(NetworkSource, IsNetworkURL) {
    EXPECT_TRUE (NetworkSource::isNetworkURL("https://cdn.com/v.mp4"));
    EXPECT_FALSE(NetworkSource::isNetworkURL("/local/file.mkv"));
}

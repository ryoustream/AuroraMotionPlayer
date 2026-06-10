#include <gtest/gtest.h>
#include "audio/AudioBuffer.h"
using namespace aurora::audio;

TEST(AudioBuffer, BytesPerSample) {
    AudioBuffer buf;
    buf.format = SampleFormat::S16;  EXPECT_EQ(buf.bytesPerSample(), 2u);
    buf.format = SampleFormat::FLT;  EXPECT_EQ(buf.bytesPerSample(), 4u);
    buf.format = SampleFormat::DBL;  EXPECT_EQ(buf.bytesPerSample(), 8u);
    buf.format = SampleFormat::U8;   EXPECT_EQ(buf.bytesPerSample(), 1u);
}

TEST(AudioBuffer, TotalBytes) {
    AudioBuffer buf;
    buf.channels  = 2;
    buf.nbSamples = 1024;
    buf.format    = SampleFormat::FLT;
    EXPECT_EQ(buf.totalBytes(), 2u * 1024u * 4u);
}

TEST(AudioBuffer, TimestampSeconds) {
    AudioBuffer buf;
    buf.pts      = 48000;
    buf.timeBase = 1.0 / 48000.0;
    EXPECT_NEAR(buf.timestampSeconds(), 1.0, 1e-9);
}

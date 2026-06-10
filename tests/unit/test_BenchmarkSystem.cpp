#include <gtest/gtest.h>
#include "benchmark/BenchmarkSystem.h"
#include <thread>
#include <chrono>

using namespace aurora::benchmark;

TEST(BenchmarkSystem, StartsAndSnapshots) {
    BenchmarkSystem bench;
    bench.start();
    bench.onFrameDecoded();
    bench.onFrameDecoded();
    bench.onRenderBegin();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    bench.onRenderEnd();
    bench.onFrameRendered();
    auto snap = bench.snapshot();
    EXPECT_EQ(snap.droppedFrames, 0u);
    bench.stop();
}

TEST(BenchmarkSystem, FormatOutput) {
    BenchmarkSnapshot s;
    s.renderFPS = 60.0;
    s.decodeFPS = 30.0;
    s.interpFPS = 60.0;
    auto str = BenchmarkSystem::format(s);
    EXPECT_NE(str.find("60.0"), std::string::npos);
    EXPECT_NE(str.find("Render"), std::string::npos);
}

TEST(BenchmarkSystem, DroppedFrameCount) {
    BenchmarkSystem bench;
    bench.start();
    bench.onFrameDropped();
    bench.onFrameDropped();
    bench.onFrameDropped();
    EXPECT_EQ(bench.snapshot().droppedFrames, 3u);
    bench.stop();
}

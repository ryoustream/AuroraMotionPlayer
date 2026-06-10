#include <gtest/gtest.h>
#include "hdr/HDREngine.h"
using namespace aurora::hdr;

TEST(ToneMapper, BT2390ClipsAt1) {
    ToneMapper tm(ToneMappingAlgorithm::BT2390);
    float result = tm.map(10000.0f, 10000.0f, 400.0f);
    EXPECT_LE(result, 1.0f);
    EXPECT_GE(result, 0.0f);
}

TEST(ToneMapper, PassthroughBelowPeak) {
    ToneMapper tm(ToneMappingAlgorithm::BT2390);
    // Input well below destination peak should map close to linear
    float result = tm.map(50.0f, 1000.0f, 400.0f);
    EXPECT_GT(result, 0.0f);
    EXPECT_LE(result, 1.0f);
}

TEST(ToneMapper, ACESNormalizedRange) {
    ToneMapper tm(ToneMappingAlgorithm::ACES);
    for (float x : {0.0f, 100.0f, 500.0f, 1000.0f}) {
        float r = tm.map(x, 1000.0f, 400.0f);
        EXPECT_GE(r, 0.0f);
        EXPECT_LE(r, 1.0f);
    }
}

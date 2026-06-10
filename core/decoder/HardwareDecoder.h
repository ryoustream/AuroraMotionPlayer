#pragma once
#include "FFmpegDecoder.h"
#include <memory>
#include <string>
#include <vector>

namespace aurora::decoder {

// ── Hardware decoder capabilities query ───────────────────────────────────────
struct HWDecoderInfo {
    HWAccelType type;
    std::string name;
    bool        available = false;
    int         deviceId  = 0;
};

class HardwareDecoder {
public:
    // Probe all available HW decoders on the current system
    static std::vector<HWDecoderInfo> enumerate();

    // Pick the best available HW accel type for the given codec
    static HWAccelType selectBest(const std::string& codecName);

    // Check if a specific HW accel is available
    static bool isAvailable(HWAccelType type);

    // Name of the HW accel type
    static const char* typeName(HWAccelType type) noexcept;
};

// ── Decoder factory ────────────────────────────────────────────────────────────
class DecoderFactory {
public:
    // Create an FFmpegDecoder with the best available HW accel
    static std::unique_ptr<FFmpegDecoder> createAuto(
        const std::string& codecName = "");

    // Create with explicit HW type
    static std::unique_ptr<FFmpegDecoder> create(
        HWAccelType hwType,
        int threadCount = 0);
};

} // namespace aurora::decoder

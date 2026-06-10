#pragma once
#include <vector>
#include <cstdint>

namespace aurora::hdr {

// Dolby Vision RPU (Reference Processing Unit) parser
// Detects DV profile from bitstream metadata
struct DolbyVisionMetadata {
    int     profile        = -1;  // -1 = not DV
    int     level          = -1;
    bool    isBaseLayerHDR = false;
    bool    isMELLayer     = false;  // Minimum Enhancement Layer
    bool    isFELLayer     = false;  // Full Enhancement Layer
    float   maxPQ          = 0.0f;
    float   minPQ          = 0.0f;
};

class DolbyVisionParser {
public:
    // Detect DV profile from codec extradata or SEI NAL
    static DolbyVisionMetadata detect(const uint8_t* extradata, int extradataSize);

    // Parse RPU from H.265 unregistered SEI
    static bool parseRPU(const uint8_t* rpu, int rpuSize, DolbyVisionMetadata& out);

    // Profile names for logging/UI
    static const char* profileName(int profile);
};

} // namespace aurora::hdr

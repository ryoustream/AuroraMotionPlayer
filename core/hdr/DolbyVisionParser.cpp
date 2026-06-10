#include "DolbyVisionParser.h"
#include <cstring>

namespace aurora::hdr {

DolbyVisionMetadata DolbyVisionParser::detect(const uint8_t* extradata, int size) {
    DolbyVisionMetadata meta;
    if (!extradata || size < 4) return meta;

    // Look for DOVI configuration record (in MP4/MKV dvcC/dvvC box)
    // Byte 0: version, Byte 1: profile+level
    if (size >= 24) {
        uint8_t profileLevel = extradata[1];
        meta.profile = (profileLevel >> 1) & 0x7F;
        meta.level   = ((extradata[1] & 0x1) << 5) | (extradata[2] >> 3);
        if (meta.profile >= 0 && meta.profile <= 9)
            return meta; // valid profile found
    }

    meta.profile = -1;
    return meta;
}

bool DolbyVisionParser::parseRPU(const uint8_t*, int, DolbyVisionMetadata&) {
    // Full RPU parsing requires implementing the DV spec
    // This is a placeholder for future implementation
    return false;
}

const char* DolbyVisionParser::profileName(int profile) {
    switch (profile) {
    case 0:  return "DV Profile 0 (Base layer BL+EL+RPU)";
    case 1:  return "DV Profile 1 (Deprecated)";
    case 2:  return "DV Profile 2 (BL+EL+RPU)";
    case 3:  return "DV Profile 3 (BL+EL, no RPU)";
    case 4:  return "DV Profile 4 (BL+EL, DV-only)";
    case 5:  return "DV Profile 5 (BL+EL+RPU, IPTPQc2)";
    case 6:  return "DV Profile 6 (HDR10 compatible)";
    case 7:  return "DV Profile 7 (MEL, HDR10 compatible)";
    case 8:  return "DV Profile 8 (BL+RPU, HDR10 compatible — most common)";
    case 9:  return "DV Profile 9 (BL+RPU, SDR compatible)";
    default: return "Unknown DV Profile";
    }
}

} // namespace aurora::hdr

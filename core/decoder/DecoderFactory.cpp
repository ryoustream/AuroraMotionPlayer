// DecoderFactory.cpp — creates appropriate decoder based on codec and HW availability
#include "FFmpegDecoder.h"
#include "HardwareDecoder.h"
#include <memory>
#include <string>

namespace aurora::decoder {

// Returns a hardware-accelerated decoder if available, else software FFmpeg
std::unique_ptr<FFmpegDecoder> createDecoder(const std::string& codecName,
                                              bool preferHardware) {
    auto dec = std::make_unique<FFmpegDecoder>();
    if (preferHardware) {
        // Try HW acceleration in priority order:
        // 1. D3D11VA (Windows)
        // 2. NVDEC (NVIDIA)
        // 3. DXVA2 (Windows legacy)
        // 4. MediaCodec (Android — handled separately)
        // For now, FFmpegDecoder handles HW context internally via AVHWDevice
        dec->setPreferHardware(true);
    }
    return dec;
}

} // namespace aurora::decoder

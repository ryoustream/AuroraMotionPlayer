#include "HardwareDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

namespace aurora::decoder {

const char* HardwareDecoder::typeName(HWAccelType type) noexcept {
    switch (type) {
    case HWAccelType::NVDEC:      return "NVDEC (NVIDIA)";
    case HWAccelType::D3D11VA:    return "D3D11VA (DirectX 11)";
    case HWAccelType::DXVA2:      return "DXVA2 (DirectX 9)";
    case HWAccelType::QSV:        return "QSV (Intel Quick Sync)";
    case HWAccelType::MediaCodec: return "MediaCodec (Android)";
    case HWAccelType::Vulkan:     return "Vulkan";
    case HWAccelType::None:       return "Software";
    default:                       return "Unknown";
    }
}

std::vector<HWDecoderInfo> HardwareDecoder::enumerate() {
    std::vector<HWDecoderInfo> result;

    // Map from FFmpeg hw type to our enum
    struct HWEntry { AVHWDeviceType ffType; HWAccelType ourType; const char* name; };
    static const HWEntry entries[] = {
#ifdef _WIN32
        { AV_HWDEVICE_TYPE_D3D11VA,  HWAccelType::D3D11VA,  "D3D11VA"  },
        { AV_HWDEVICE_TYPE_DXVA2,    HWAccelType::DXVA2,    "DXVA2"    },
        { AV_HWDEVICE_TYPE_CUDA,     HWAccelType::NVDEC,    "NVDEC"    },
        { AV_HWDEVICE_TYPE_QSV,      HWAccelType::QSV,      "QSV"      },
#endif
#ifdef __ANDROID__
        { AV_HWDEVICE_TYPE_MEDIACODEC, HWAccelType::MediaCodec, "MediaCodec" },
#endif
        { AV_HWDEVICE_TYPE_VULKAN,   HWAccelType::Vulkan,   "Vulkan"   },
    };

    for (const auto& e : entries) {
        HWDecoderInfo info;
        info.type = e.ourType;
        info.name = e.name;

        // Try to create the device context to verify availability
        AVBufferRef* ctx = nullptr;
        info.available = (av_hwdevice_ctx_create(&ctx, e.ffType,
                                                  nullptr, nullptr, 0) == 0);
        if (ctx) av_buffer_unref(&ctx);

        result.push_back(info);
    }

    return result;
}

bool HardwareDecoder::isAvailable(HWAccelType type) {
    AVHWDeviceType ffType = AV_HWDEVICE_TYPE_NONE;
    switch (type) {
#ifdef _WIN32
    case HWAccelType::D3D11VA:    ffType = AV_HWDEVICE_TYPE_D3D11VA;  break;
    case HWAccelType::DXVA2:      ffType = AV_HWDEVICE_TYPE_DXVA2;    break;
    case HWAccelType::NVDEC:      ffType = AV_HWDEVICE_TYPE_CUDA;     break;
    case HWAccelType::QSV:        ffType = AV_HWDEVICE_TYPE_QSV;      break;
#endif
#ifdef __ANDROID__
    case HWAccelType::MediaCodec: ffType = AV_HWDEVICE_TYPE_MEDIACODEC; break;
#endif
    case HWAccelType::Vulkan:     ffType = AV_HWDEVICE_TYPE_VULKAN;   break;
    default: return false;
    }
    AVBufferRef* ctx = nullptr;
    bool ok = (av_hwdevice_ctx_create(&ctx, ffType, nullptr, nullptr, 0) == 0);
    if (ctx) av_buffer_unref(&ctx);
    return ok;
}

HWAccelType HardwareDecoder::selectBest(const std::string& /*codecName*/) {
    // Priority order: D3D11VA > NVDEC > QSV > Vulkan > Software
#ifdef _WIN32
    if (isAvailable(HWAccelType::D3D11VA)) return HWAccelType::D3D11VA;
    if (isAvailable(HWAccelType::NVDEC))   return HWAccelType::NVDEC;
    if (isAvailable(HWAccelType::QSV))     return HWAccelType::QSV;
#endif
#ifdef __ANDROID__
    if (isAvailable(HWAccelType::MediaCodec)) return HWAccelType::MediaCodec;
#endif
    if (isAvailable(HWAccelType::Vulkan))  return HWAccelType::Vulkan;
    return HWAccelType::None;
}

// ── DecoderFactory ─────────────────────────────────────────────────────────────
std::unique_ptr<FFmpegDecoder> DecoderFactory::createAuto(
    const std::string& codecName)
{
    DecoderConfig cfg;
    cfg.hwAccel = HardwareDecoder::selectBest(codecName);
    return std::make_unique<FFmpegDecoder>(cfg);
}

std::unique_ptr<FFmpegDecoder> DecoderFactory::create(
    HWAccelType hwType, int threadCount)
{
    DecoderConfig cfg;
    cfg.hwAccel      = hwType;
    cfg.threadCount  = threadCount;
    return std::make_unique<FFmpegDecoder>(cfg);
}

} // namespace aurora::decoder

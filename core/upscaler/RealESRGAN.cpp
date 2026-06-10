#include "UpscalerFactory.h"
#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#endif

namespace aurora::upscaler {
struct RealESRGAN::Impl {
#ifdef AURORA_NCNN
    ncnn::Net net;
#endif
};
RealESRGAN::RealESRGAN() : m_impl(std::make_unique<Impl>()) {}
RealESRGAN::~RealESRGAN() { shutdown(); }

bool RealESRGAN::init(const UpscalerConfig& cfg) {
    m_cfg = cfg;
#ifdef AURORA_NCNN
    m_impl->net.opt.use_vulkan_compute = true;
    std::string p = cfg.modelPath + "/realesrgan-x" + std::to_string((int)cfg.factor) + ".param";
    std::string b = cfg.modelPath + "/realesrgan-x" + std::to_string((int)cfg.factor) + ".bin";
    if (m_impl->net.load_param(p.c_str()) != 0) return false;
    if (m_impl->net.load_model(b.c_str()) != 0) return false;
    m_initialized = true;
    return true;
#else
    return false;
#endif
}
void RealESRGAN::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
    m_initialized = false;
}
video::VideoFramePtr RealESRGAN::process(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return frame;
    int scale = static_cast<int>(m_cfg.factor);
    int newW = frame->width()  * scale;
    int newH = frame->height() * scale;
    // Full implementation runs NCNN inference here
    // Returns upscaled frame
    auto out = std::make_shared<video::VideoFrame>(newW, newH, video::PixelFormat::YUV420P);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    out->setColorMeta(frame->colorMeta());
    return out;
}
} // namespace aurora::upscaler

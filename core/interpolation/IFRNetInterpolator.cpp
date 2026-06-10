#include "IFRNetInterpolator.h"
#include "AuroraFlow.h"
#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#endif

namespace aurora::interpolation {
struct IFRNetInterpolator::Impl {
#ifdef AURORA_NCNN
    ncnn::Net net;
#endif
};
IFRNetInterpolator::IFRNetInterpolator() : m_impl(std::make_unique<Impl>()) {}
IFRNetInterpolator::~IFRNetInterpolator() { shutdown(); }

bool IFRNetInterpolator::init(const InterpolationConfig& cfg) {
#ifdef AURORA_NCNN
    m_impl->net.opt.use_vulkan_compute = true;
    std::string p = cfg.modelPath + "/ifrnet.param";
    std::string b = cfg.modelPath + "/ifrnet.bin";
    if (m_impl->net.load_param(p.c_str()) != 0) return false;
    if (m_impl->net.load_model(b.c_str()) != 0) return false;
    m_initialized = true;
    return true;
#else
    return false;
#endif
}

void IFRNetInterpolator::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
    m_initialized = false;
}

video::VideoFramePtr IFRNetInterpolator::interpolate(
    video::VideoFramePtr f0, video::VideoFramePtr f1, float t)
{
    if (!m_initialized) return nullptr;
    // Inference logic mirrors RIFEInterpolator with IFRNet-specific I/O names
    (void)f0; (void)f1; (void)t;
    return nullptr; // TODO: wire up NCNN inference
}
} // namespace aurora::interpolation

#include "FILMInterpolator.h"
#include "AuroraFlow.h"
#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

namespace aurora::interpolation {
struct FILMInterpolator::Impl {
#ifdef AURORA_ONNX
    std::unique_ptr<Ort::Session> session;
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "film"};
    Ort::SessionOptions opts;
#endif
};
FILMInterpolator::FILMInterpolator() : m_impl(std::make_unique<Impl>()) {}
FILMInterpolator::~FILMInterpolator() { shutdown(); }

bool FILMInterpolator::init(const InterpolationConfig& cfg) {
#ifdef AURORA_ONNX
    std::string modelFile = cfg.modelPath + "/film.onnx";
    m_impl->opts.SetIntraOpNumThreads(4);
    m_impl->opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_impl->session = std::make_unique<Ort::Session>(
        m_impl->env, modelFile.c_str(), m_impl->opts);
    m_initialized = true;
    return true;
#else
    return false;
#endif
}

void FILMInterpolator::shutdown() { m_initialized = false; }

video::VideoFramePtr FILMInterpolator::interpolate(
    video::VideoFramePtr f0, video::VideoFramePtr f1, float t)
{
    if (!m_initialized) return nullptr;
    (void)f0; (void)f1; (void)t;
    return nullptr;
}
} // namespace aurora::interpolation

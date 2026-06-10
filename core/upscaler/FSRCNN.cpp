#include "UpscalerFactory.h"
namespace aurora::upscaler {
struct FSRCNN::Impl {};
FSRCNN::FSRCNN() : m_impl(std::make_unique<Impl>()) {}
FSRCNN::~FSRCNN() { shutdown(); }
bool FSRCNN::init(const UpscalerConfig&) { m_initialized = true; return true; }
void FSRCNN::shutdown() { m_initialized = false; }
video::VideoFramePtr FSRCNN::process(video::VideoFramePtr frame) { return frame; }
} // namespace aurora::upscaler

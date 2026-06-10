#include "UpscalerFactory.h"
namespace aurora::upscaler {
struct SPAN::Impl {};
SPAN::SPAN() : m_impl(std::make_unique<Impl>()) {}
SPAN::~SPAN() { shutdown(); }
bool SPAN::init(const UpscalerConfig&) { m_initialized = true; return true; }
void SPAN::shutdown() { m_initialized = false; }
video::VideoFramePtr SPAN::process(video::VideoFramePtr frame) { return frame; }
} // namespace aurora::upscaler

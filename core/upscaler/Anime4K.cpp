#include "UpscalerFactory.h"
namespace aurora::upscaler {
Anime4K::Anime4K() = default;
Anime4K::~Anime4K() { shutdown(); }
bool Anime4K::init(const UpscalerConfig& cfg) { m_cfg = cfg; m_initialized = true; return true; }
void Anime4K::shutdown() { m_initialized = false; }
video::VideoFramePtr Anime4K::process(video::VideoFramePtr frame) { return frame; }
} // namespace aurora::upscaler

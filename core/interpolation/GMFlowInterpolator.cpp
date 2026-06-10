#include "GMFlowInterpolator.h"
#include "AuroraFlow.h"

namespace aurora::interpolation {
struct GMFlowInterpolator::Impl {};
GMFlowInterpolator::GMFlowInterpolator() : m_impl(std::make_unique<Impl>()) {}
GMFlowInterpolator::~GMFlowInterpolator() { shutdown(); }
bool GMFlowInterpolator::init(const InterpolationConfig&) { m_initialized = true; return true; }
void GMFlowInterpolator::shutdown() { m_initialized = false; }
video::VideoFramePtr GMFlowInterpolator::interpolate(
    video::VideoFramePtr, video::VideoFramePtr, float) { return nullptr; }
} // namespace aurora::interpolation

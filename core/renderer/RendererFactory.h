#pragma once
#include "RendererBase.h"
#include <memory>

namespace aurora::renderer {

// Creates the best available renderer for the current platform and config.
// Priority: Vulkan → DX12 → DX11 → OpenGL
std::unique_ptr<RendererBase> createBestRenderer(const RendererConfig& cfg);
std::unique_ptr<RendererBase> createRenderer(RendererBackend backend);

} // namespace aurora::renderer

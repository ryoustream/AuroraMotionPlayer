#include "RendererFactory.h"
#include "VulkanRenderer.h"
#include "DX11Renderer.h"
#include "OpenGLRenderer.h"
#include <cstdio>

namespace aurora::renderer {

std::unique_ptr<RendererBase> createRenderer(RendererBackend backend) {
    switch (backend) {
    case RendererBackend::Vulkan:
        return std::make_unique<VulkanRenderer>();
    case RendererBackend::DirectX11:
        return std::make_unique<DX11Renderer>();
    case RendererBackend::OpenGL:
    case RendererBackend::OpenGLES:
        return std::make_unique<OpenGLRenderer>();
    default:
        return nullptr;
    }
}

std::unique_ptr<RendererBase> createBestRenderer(const RendererConfig& cfg) {
    // Try in priority order: Vulkan → DX11 → OpenGL
    std::vector<RendererBackend> order = {
#ifdef AURORA_VULKAN
        RendererBackend::Vulkan,
#endif
#ifdef _WIN32
        RendererBackend::DirectX11,
#endif
        RendererBackend::OpenGL,
    };

    for (auto backend : order) {
        auto r = createRenderer(backend);
        if (!r) continue;
        RendererConfig tryConfig = cfg;
        tryConfig.backend = backend;
        // Headless probe (null handle) — real init happens later
        fprintf(stderr, "[Renderer] Trying backend %d\n", static_cast<int>(backend));
        return r;
    }
    return nullptr;
}

} // namespace aurora::renderer

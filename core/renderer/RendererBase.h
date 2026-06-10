#pragma once
#include "video/VideoFrame.h"
#include <string>
#include <functional>

namespace aurora::renderer {

enum class RendererBackend {
    Vulkan,
    DirectX12,
    DirectX11,
    OpenGL,
    OpenGLES,
};

struct RendererConfig {
    RendererBackend backend      = RendererBackend::Vulkan;
    int             width        = 1920;
    int             height       = 1080;
    bool            vsync        = true;
    bool            hdrOutput    = false;
    int             gpuDeviceId  = 0;
    bool            exclusiveFS  = false;
};

class RendererBase {
public:
    virtual ~RendererBase() = default;

    virtual bool init(void* nativeWindowHandle, const RendererConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual void renderFrame(aurora::video::VideoFramePtr frame) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void setHDRMetadata(float maxLuminance, float minLuminance) = 0;
    virtual void present() = 0;

    RendererBackend backend() const noexcept { return m_backend; }
    bool isInitialized() const noexcept      { return m_initialized; }

    using FramePresentedCallback = std::function<void()>;
    void setFramePresentedCallback(FramePresentedCallback cb) {
        m_presentedCb = std::move(cb);
    }

protected:
    RendererBackend         m_backend       = RendererBackend::Vulkan;
    bool                    m_initialized   = false;
    FramePresentedCallback  m_presentedCb;
};

} // namespace aurora::renderer

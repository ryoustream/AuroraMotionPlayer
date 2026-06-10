#pragma once
#include "RendererBase.h"

namespace aurora::renderer {
class OpenGLRenderer : public RendererBase {
public:
    OpenGLRenderer();
    ~OpenGLRenderer() override;
    bool init(void* nativeWindowHandle, const RendererConfig& cfg) override;
    void shutdown() override;
    void renderFrame(video::VideoFramePtr frame) override;
    void resize(int width, int height) override;
    void setHDRMetadata(float maxLuminance, float minLuminance) override;
    void present() override;
private:
    void* m_glContext = nullptr;
    unsigned m_yuvTex[3] = {};
    unsigned m_vao = 0, m_vbo = 0, m_shader = 0;
};
} // namespace aurora::renderer

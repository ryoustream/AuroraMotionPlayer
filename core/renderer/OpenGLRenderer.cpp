#include "OpenGLRenderer.h"
namespace aurora::renderer {
OpenGLRenderer::OpenGLRenderer() { m_backend = RendererBackend::OpenGL; }
OpenGLRenderer::~OpenGLRenderer() { shutdown(); }
bool OpenGLRenderer::init(void*, const RendererConfig&) { m_initialized = true; return true; }
void OpenGLRenderer::shutdown() { m_initialized = false; }
void OpenGLRenderer::renderFrame(video::VideoFramePtr) {}
void OpenGLRenderer::resize(int, int) {}
void OpenGLRenderer::setHDRMetadata(float, float) {}
void OpenGLRenderer::present() { if (m_presentedCb) m_presentedCb(); }
} // namespace aurora::renderer

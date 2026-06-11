#pragma once
#include "RendererBase.h"
#ifdef _WIN32
#  include <d3d11.h>
#  include <dxgi1_4.h>
#  include <d3dcompiler.h>
#  pragma comment(lib, "d3d11.lib")
#  pragma comment(lib, "dxgi.lib")
#  pragma comment(lib, "d3dcompiler.lib")
#endif
#include <memory>
#include <vector>

namespace aurora::renderer {

class DX11Renderer : public RendererBase {
public:
    DX11Renderer();
    ~DX11Renderer() override;

    bool init(void* nativeWindowHandle, const RendererConfig& cfg) override;
    void shutdown() override;
    void renderFrame(video::VideoFramePtr frame) override;
    void resize(int width, int height) override;
    void setHDRMetadata(float maxLuminance, float minLuminance) override;
    void present() override;

private:
    bool createDeviceAndSwapchain(void* hwnd, int w, int h);
    bool createShadersAndLayout();
    bool createVideoTextures(int w, int h);
    bool createSamplerAndCB();
    void uploadYUVFrame(video::VideoFramePtr frame);
    void resizeSwapchain(int w, int h);

#ifdef _WIN32
    ID3D11Device*           m_device        = nullptr;
    ID3D11DeviceContext*    m_ctx           = nullptr;
    IDXGISwapChain1*        m_swapChain     = nullptr;
    ID3D11RenderTargetView* m_rtv           = nullptr;
    ID3D11VertexShader*     m_vertShader    = nullptr;
    ID3D11PixelShader*      m_pixShader     = nullptr;
    ID3D11InputLayout*      m_inputLayout   = nullptr;
    ID3D11SamplerState*     m_sampler       = nullptr;
    ID3D11Buffer*           m_cbuf          = nullptr;  // constant buffer

    // YUV plane textures
    ID3D11Texture2D*          m_texY         = nullptr;
    ID3D11Texture2D*          m_texU         = nullptr;
    ID3D11Texture2D*          m_texV         = nullptr;
    ID3D11ShaderResourceView* m_srvY         = nullptr;
    ID3D11ShaderResourceView* m_srvU         = nullptr;
    ID3D11ShaderResourceView* m_srvV         = nullptr;
#endif

    int  m_width      = 0;
    int  m_height     = 0;
    bool m_hdrEnabled = false;

    // HLSL source
    static const char* kVertHLSL;
    static const char* kPixelHLSL;
};

} // namespace aurora::renderer

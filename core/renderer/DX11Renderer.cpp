// DX11Renderer.cpp
// Direct3D 11 fallback renderer for Aurora Motion Player (Windows).
// Used when Vulkan is unavailable (older GPUs, DX11-only systems).
//
// Pipeline: CPU YUV420P → D3D11 textures (Y/U/V) → HLSL pixel shader → present

#include "DX11Renderer.h"
#include <cstring>
#include <cstdio>

#ifdef _WIN32

namespace aurora::renderer {

// ── HLSL source ──────────────────────────────────────────────────────────────
const char* DX11Renderer::kVertHLSL = R"HLSL(
// Fullscreen triangle — no VBO
void VS(uint vid : SV_VertexID,
        out float4 pos : SV_POSITION,
        out float2 uv  : TEXCOORD0)
{
    uv  = float2((vid << 1) & 2, vid & 2);
    pos = float4(uv * float2(2,-2) + float2(-1,1), 0, 1);
}
)HLSL";

const char* DX11Renderer::kPixelHLSL = R"HLSL(
Texture2D    texY : register(t0);
Texture2D    texU : register(t1);
Texture2D    texV : register(t2);
SamplerState smp  : register(s0);

cbuffer VideoCB : register(b0) {
    float4x4 colorMatrix;  // 3x3 packed into 4x4
    float    brightness;
    float    contrast;
    float    saturation;
    float    hdrEnabled;
    float    hdrPeak;
    float3   _pad;
};

// PQ EOTF → linear
float PQtoLinear(float x) {
    float m1 = 0.1593017578125, m2 = 78.84375;
    float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    float xp = pow(max(x, 0.0), 1.0/m2);
    return pow(max(xp - c1, 0.0) / (c2 - c3*xp), 1.0/m1);
}

float4 PS(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float Y  = texY.Sample(smp, uv).r - (16.0/255.0);
    float Cb = texU.Sample(smp, uv).r - (128.0/255.0);
    float Cr = texV.Sample(smp, uv).r - (128.0/255.0);

    // BT.709 matrix (can override via cbuffer in future)
    float3 rgb;
    rgb.r = 1.164*Y + 0.000*Cb + 1.793*Cr;
    rgb.g = 1.164*Y - 0.213*Cb - 0.533*Cr;
    rgb.b = 1.164*Y + 2.112*Cb + 0.000*Cr;
    rgb   = saturate(rgb);

    if (hdrEnabled > 0.5) {
        float peak = hdrPeak / 10000.0;
        // Reinhard on linear PQ
        rgb.r = PQtoLinear(rgb.r); rgb.r = rgb.r*(1+rgb.r/(peak*peak))/(1+rgb.r);
        rgb.g = PQtoLinear(rgb.g); rgb.g = rgb.g*(1+rgb.g/(peak*peak))/(1+rgb.g);
        rgb.b = PQtoLinear(rgb.b); rgb.b = rgb.b*(1+rgb.b/(peak*peak))/(1+rgb.b);
        rgb   = pow(saturate(rgb), 1.0/2.2);
    }

    rgb = saturate((rgb - 0.5)*contrast + 0.5 + brightness);
    return float4(rgb, 1.0);
}
)HLSL";

// ── CB layout ────────────────────────────────────────────────────────────────
struct VideoCB {
    float colorMatrix[16];
    float brightness  = 0.0f;
    float contrast    = 1.0f;
    float saturation  = 1.0f;
    float hdrEnabled  = 0.0f;
    float hdrPeak     = 1000.0f;
    float _pad[3]     = {};
};

// ── Constructor / Destructor ─────────────────────────────────────────────────
DX11Renderer::DX11Renderer() {
    m_backend = RendererBackend::DirectX11;
}

DX11Renderer::~DX11Renderer() {
    shutdown();
}

// ── init ─────────────────────────────────────────────────────────────────────
bool DX11Renderer::init(void* nativeWindowHandle, const RendererConfig& cfg) {
    m_width      = cfg.width;
    m_height     = cfg.height;
    m_hdrEnabled = cfg.hdrOutput;

    if (!createDeviceAndSwapchain(nativeWindowHandle, cfg.width, cfg.height)) {
        fprintf(stderr, "[DX11] createDeviceAndSwapchain failed\n"); return false;
    }
    if (!createShadersAndLayout()) {
        fprintf(stderr, "[DX11] createShaders failed\n"); return false;
    }
    if (!createVideoTextures(cfg.width, cfg.height)) {
        fprintf(stderr, "[DX11] createVideoTextures failed\n"); return false;
    }
    if (!createSamplerAndCB()) {
        fprintf(stderr, "[DX11] createSamplerAndCB failed\n"); return false;
    }

    m_initialized = true;
    return true;
}

// ── createDeviceAndSwapchain ──────────────────────────────────────────────────
bool DX11Renderer::createDeviceAndSwapchain(void* hwnd, int w, int h) {
    if (!hwnd) return false;
    HWND hWnd = reinterpret_cast<HWND>(hwnd);

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1
    };
    D3D_FEATURE_LEVEL chosen;
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &m_device, &chosen, &m_ctx);

    if (FAILED(hr)) {
        // Fallback to WARP software renderer
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                               &m_device, &chosen, &m_ctx);
        if (FAILED(hr)) return false;
        fprintf(stderr, "[DX11] Using WARP software renderer\n");
    }

    // Create DXGI factory and swapchain
    IDXGIDevice*  dxgiDev     = nullptr;
    IDXGIAdapter* dxgiAdapter = nullptr;
    IDXGIFactory2* dxgiFactory = nullptr;
    m_device->QueryInterface(__uuidof(IDXGIDevice),  (void**)&dxgiDev);
    dxgiDev->GetAdapter(&dxgiAdapter);
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);

    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width       = static_cast<UINT>(w);
    scd.Height      = static_cast<UINT>(h);
    scd.Format      = m_hdrEnabled ? DXGI_FORMAT_R10G10B10A2_UNORM
                                   : DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc  = {1, 0};
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Scaling     = DXGI_SCALING_STRETCH;
    scd.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;

    hr = dxgiFactory->CreateSwapChainForHwnd(m_device, hWnd, &scd,
                                              nullptr, nullptr, &m_swapChain);
    dxgiFactory->Release();
    dxgiAdapter->Release();
    dxgiDev->Release();
    if (FAILED(hr)) return false;

    // Create RTV from back buffer
    ID3D11Texture2D* backBuf = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuf);
    hr = m_device->CreateRenderTargetView(backBuf, nullptr, &m_rtv);
    backBuf->Release();
    return SUCCEEDED(hr);
}

// ── createShadersAndLayout ────────────────────────────────────────────────────
bool DX11Renderer::createShadersAndLayout() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errBlob = nullptr;

    HRESULT hr = D3DCompile(kVertHLSL, strlen(kVertHLSL), "VS", nullptr, nullptr,
                            "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            fprintf(stderr, "[DX11] VS compile: %s\n",
                    (char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        return false;
    }

    hr = D3DCompile(kPixelHLSL, strlen(kPixelHLSL), "PS", nullptr, nullptr,
                    "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            fprintf(stderr, "[DX11] PS compile: %s\n",
                    (char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        vsBlob->Release();
        return false;
    }

    hr  = m_device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                        vsBlob->GetBufferSize(), nullptr, &m_vertShader);
    hr |= m_device->CreatePixelShader(psBlob->GetBufferPointer(),
                                       psBlob->GetBufferSize(), nullptr, &m_pixShader);
    vsBlob->Release();
    psBlob->Release();
    return SUCCEEDED(hr);
}

// ── createVideoTextures ───────────────────────────────────────────────────────
bool DX11Renderer::createVideoTextures(int w, int h) {
    auto makeTexSRV = [&](int tw, int th,
                           ID3D11Texture2D** outTex,
                           ID3D11ShaderResourceView** outSRV) -> bool
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width            = static_cast<UINT>(tw);
        td.Height           = static_cast<UINT>(th);
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8_UNORM;
        td.SampleDesc       = {1, 0};
        td.Usage            = D3D11_USAGE_DYNAMIC;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateTexture2D(&td, nullptr, outTex);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format                    = DXGI_FORMAT_R8_UNORM;
        srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels       = 1;
        hr = m_device->CreateShaderResourceView(*outTex, &srvd, outSRV);
        return SUCCEEDED(hr);
    };

    return makeTexSRV(w,   h,   &m_texY, &m_srvY)
        && makeTexSRV(w/2, h/2, &m_texU, &m_srvU)
        && makeTexSRV(w/2, h/2, &m_texV, &m_srvV);
}

// ── createSamplerAndCB ────────────────────────────────────────────────────────
bool DX11Renderer::createSamplerAndCB() {
    D3D11_SAMPLER_DESC sd{};
    sd.Filter         = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    HRESULT hr = m_device->CreateSamplerState(&sd, &m_sampler);
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth      = (sizeof(VideoCB) + 15) & ~15;  // 16-byte aligned
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&bd, nullptr, &m_cbuf);
    return SUCCEEDED(hr);
}

// ── uploadYUVFrame ────────────────────────────────────────────────────────────
void DX11Renderer::uploadYUVFrame(video::VideoFramePtr frame) {
    if (!frame) return;

    auto uploadPlane = [&](ID3D11Texture2D* tex,
                            const uint8_t* src, int srcStride,
                            int w, int h)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_ctx->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            for (int y = 0; y < h; ++y) {
                memcpy(static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch,
                       src + y * srcStride, static_cast<size_t>(w));
            }
            m_ctx->Unmap(tex, 0);
        }
    };

    int w = frame->width(), h = frame->height();
    uploadPlane(m_texY, frame->data(0), frame->linesize(0), w,   h  );
    uploadPlane(m_texU, frame->data(1), frame->linesize(1), w/2, h/2);
    uploadPlane(m_texV, frame->data(2), frame->linesize(2), w/2, h/2);

    // Update constant buffer
    VideoCB cb{};
    // BT.709 color matrix (identity for now, shader has it hardcoded)
    cb.brightness = 0.0f;
    cb.contrast   = 1.0f;
    cb.hdrEnabled = m_hdrEnabled ? 1.0f : 0.0f;
    cb.hdrPeak    = frame->colorMeta().masterMaxLum > 0 ?
                    frame->colorMeta().masterMaxLum : 1000.0f;

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(m_ctx->Map(m_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, &cb, sizeof(cb));
        m_ctx->Unmap(m_cbuf, 0);
    }
}

// ── renderFrame ───────────────────────────────────────────────────────────────
void DX11Renderer::renderFrame(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return;

    uploadYUVFrame(frame);

    // Set RTV + clear to black
    const float clear[4] = {0, 0, 0, 1};
    m_ctx->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_ctx->ClearRenderTargetView(m_rtv, clear);

    // Viewport
    D3D11_VIEWPORT vp{};
    vp.Width  = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;
    m_ctx->RSSetViewports(1, &vp);

    // Bind shaders
    m_ctx->VSSetShader(m_vertShader, nullptr, 0);
    m_ctx->PSSetShader(m_pixShader,  nullptr, 0);

    // Bind textures + sampler
    ID3D11ShaderResourceView* srvs[3] = {m_srvY, m_srvU, m_srvV};
    m_ctx->PSSetShaderResources(0, 3, srvs);
    m_ctx->PSSetSamplers(0, 1, &m_sampler);
    m_ctx->PSSetConstantBuffers(0, 1, &m_cbuf);

    // Draw fullscreen triangle (3 vertices, no VBO)
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_ctx->Draw(3, 0);
}

// ── present ───────────────────────────────────────────────────────────────────
void DX11Renderer::present() {
    if (!m_initialized) return;
    m_swapChain->Present(1, 0);  // vsync
    if (m_presentedCb) m_presentedCb();
}

// ── resize ────────────────────────────────────────────────────────────────────
void DX11Renderer::resize(int w, int h) {
    if (!m_initialized || (w == m_width && h == m_height)) return;
    m_width = w; m_height = h;
    resizeSwapchain(w, h);
}

void DX11Renderer::resizeSwapchain(int w, int h) {
    m_ctx->OMSetRenderTargets(0, nullptr, nullptr);
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }

    HRESULT hr = m_swapChain->ResizeBuffers(2,
        static_cast<UINT>(w), static_cast<UINT>(h),
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) { fprintf(stderr, "[DX11] ResizeBuffers failed\n"); return; }

    ID3D11Texture2D* backBuf = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuf);
    m_device->CreateRenderTargetView(backBuf, nullptr, &m_rtv);
    backBuf->Release();
}

// ── setHDRMetadata ────────────────────────────────────────────────────────────
void DX11Renderer::setHDRMetadata(float maxLuminance, float /*minLuminance*/) {
    m_hdrEnabled = (maxLuminance > 100.0f);
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void DX11Renderer::shutdown() {
#define SAFE_RELEASE(p) if(p) { p->Release(); p = nullptr; }
    SAFE_RELEASE(m_srvY) SAFE_RELEASE(m_srvU) SAFE_RELEASE(m_srvV)
    SAFE_RELEASE(m_texY) SAFE_RELEASE(m_texU) SAFE_RELEASE(m_texV)
    SAFE_RELEASE(m_sampler) SAFE_RELEASE(m_cbuf)
    SAFE_RELEASE(m_vertShader) SAFE_RELEASE(m_pixShader) SAFE_RELEASE(m_inputLayout)
    SAFE_RELEASE(m_rtv)
    SAFE_RELEASE(m_swapChain)
    SAFE_RELEASE(m_ctx)
    SAFE_RELEASE(m_device)
#undef SAFE_RELEASE
    m_initialized = false;
}

} // namespace aurora::renderer

#else // !_WIN32

// Non-Windows stub
namespace aurora::renderer {
DX11Renderer::DX11Renderer() { m_backend = RendererBackend::DirectX11; }
DX11Renderer::~DX11Renderer() {}
bool DX11Renderer::init(void*, const RendererConfig&) { return false; }
void DX11Renderer::shutdown() {}
void DX11Renderer::renderFrame(video::VideoFramePtr) {}
void DX11Renderer::resize(int, int) {}
void DX11Renderer::setHDRMetadata(float, float) {}
void DX11Renderer::present() {}
bool DX11Renderer::createDeviceAndSwapchain(void*, int, int) { return false; }
bool DX11Renderer::createShadersAndLayout() { return false; }
bool DX11Renderer::createVideoTextures(int, int) { return false; }
bool DX11Renderer::createSamplerAndCB() { return false; }
void DX11Renderer::uploadYUVFrame(video::VideoFramePtr) {}
void DX11Renderer::resizeSwapchain(int, int) {}
const char* DX11Renderer::kVertHLSL  = "";
const char* DX11Renderer::kPixelHLSL = "";
} // namespace aurora::renderer

#endif

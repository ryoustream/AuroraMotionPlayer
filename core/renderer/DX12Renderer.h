#pragma once
/**
 * Aurora Motion Player — DirectX 12 Renderer
 *
 * Features:
 *  - Triple-buffered swap chain
 *  - YUV → RGB conversion via compute shader
 *  - HDR10 output (DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
 *  - Descriptor heap management
 *  - Async compute for AI post-processing
 */

#ifdef _WIN32

#include "RendererBase.h"
#include "video/VideoFrame.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>

using Microsoft::WRL::ComPtr;

namespace aurora::renderer {

// ── Constants ─────────────────────────────────────────────────────────────────
constexpr UINT k_FrameCount  = 3;  // Triple buffering
constexpr UINT k_RtvHeapSize = k_FrameCount + 1;
constexpr UINT k_SrvHeapSize = 32; // CBV/SRV/UAV descriptor heap

// ── DX12 Renderer ─────────────────────────────────────────────────────────────
class DX12Renderer : public RendererBase {
public:
    DX12Renderer();
    ~DX12Renderer() override;

    // RendererBase interface
    bool init(void* nativeWindowHandle, const RendererConfig& cfg) override;
    void shutdown() override;
    void renderFrame(video::VideoFramePtr frame) override;
    void resize(int width, int height) override;
    void setHDRMetadata(float maxLuminance, float minLuminance) override;
    void present() override;

    // DX12-specific
    ID3D12Device*       device()       const { return m_device.Get(); }
    ID3D12CommandQueue* commandQueue() const { return m_cmdQueue.Get(); }

private:
    // ----- Initialization helpers -------------------------------------------
    bool createDevice();
    bool createCommandQueue();
    bool createSwapChain(HWND hwnd, int width, int height);
    bool createDescriptorHeaps();
    bool createRenderTargets();
    bool createDepthStencil(int width, int height);
    bool createRootSignature();
    bool createPipelineState();
    bool createCommandAllocators();
    bool createFrameResources();
    bool createYUVTextures(int width, int height);
    bool createVertexBuffer();
    void waitForGPU();
    void moveToNextFrame();

    // ----- Per-frame rendering ----------------------------------------------
    void uploadYUVFrame(const VideoFrame& frame);
    void recordCommandList(UINT frameIdx);
    void submitAndPresent(UINT frameIdx);

    // ----- HDR --------------------------------------------------------------
    void enableHDROutput();

    // ----- Device & DXGI ----------------------------------------------------
    ComPtr<IDXGIFactory6>            m_factory;
    ComPtr<IDXGIAdapter1>            m_adapter;
    ComPtr<ID3D12Device>             m_device;
    ComPtr<ID3D12CommandQueue>       m_cmdQueue;
    ComPtr<IDXGISwapChain4>          m_swapChain;

    // ----- Descriptor heaps -------------------------------------------------
    ComPtr<ID3D12DescriptorHeap>     m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap>     m_srvHeap;
    ComPtr<ID3D12DescriptorHeap>     m_dsvHeap;
    UINT                             m_rtvDescSize = 0;
    UINT                             m_srvDescSize = 0;

    // ----- Swap chain resources ---------------------------------------------
    std::array<ComPtr<ID3D12Resource>, k_FrameCount> m_renderTargets;
    ComPtr<ID3D12Resource>                            m_depthStencil;

    // ----- Command infrastructure -------------------------------------------
    std::array<ComPtr<ID3D12CommandAllocator>, k_FrameCount> m_cmdAllocators;
    ComPtr<ID3D12GraphicsCommandList>                         m_cmdList;

    // ----- Synchronization --------------------------------------------------
    ComPtr<ID3D12Fence>  m_fence;
    UINT64               m_fenceValues[k_FrameCount] = {};
    HANDLE               m_fenceEvent = nullptr;
    UINT                 m_frameIndex = 0;

    // ----- Pipeline state ---------------------------------------------------
    ComPtr<ID3D12RootSignature>  m_rootSignature;
    ComPtr<ID3D12PipelineState>  m_pipelineState;

    // ----- Vertex buffer (fullscreen quad) ----------------------------------
    ComPtr<ID3D12Resource>  m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};

    // ----- YUV textures -----------------------------------------------------
    ComPtr<ID3D12Resource>  m_texY;     // Luma plane
    ComPtr<ID3D12Resource>  m_texUV;    // Chroma plane (NV12)
    ComPtr<ID3D12Resource>  m_uploadBuf;
    SIZE_T                  m_uploadBufSize = 0;

    // ----- State ------------------------------------------------------------
    int   m_width    = 0;
    int   m_height   = 0;
    bool  m_hdrMode  = false;
    bool  m_vsync    = true;
    bool  m_initialized = false;
};

} // namespace aurora::renderer

#endif // _WIN32

/**
 * Aurora Motion Player — DirectX 12 Renderer Implementation
 */

#ifdef _WIN32

#include "DX12Renderer.h"
#include "../video/VideoFrame.h"

#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace aurora::core {

// ── Fullscreen quad vertex ────────────────────────────────────────────────────
struct QuadVertex {
    float pos[3];
    float uv[2];
};

static constexpr QuadVertex k_Quad[] = {
    { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
    { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
};

// ── YUV → RGB HLSL ───────────────────────────────────────────────────────────
static const char* k_HLSL = R"(
Texture2D<float>  texY  : register(t0);
Texture2D<float2> texUV : register(t1);
SamplerState      samp  : register(s0);

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

VSOut VSMain(float3 pos : POSITION, float2 uv : TEXCOORD) {
    VSOut o;
    o.pos = float4(pos, 1.0);
    o.uv  = uv;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
    float  y  = texY .Sample(samp, i.uv);
    float2 uv = texUV.Sample(samp, i.uv) - 0.5;

    // BT.709 YCbCr → RGB
    float r = y + 1.5748 * uv.y;
    float g = y - 0.1873 * uv.x - 0.4681 * uv.y;
    float b = y + 1.8556 * uv.x;

    return float4(saturate(float3(r, g, b)), 1.0);
}
)";

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
DX12Renderer::DX12Renderer()  = default;
DX12Renderer::~DX12Renderer() { shutdown(); }

// ── Initialize ───────────────────────────────────────────────────────────────
bool DX12Renderer::initialize(void* windowHandle, int width, int height) {
    m_width  = width;
    m_height = height;
    HWND hwnd = static_cast<HWND>(windowHandle);

    if (!createDevice())              return false;
    if (!createCommandQueue())        return false;
    if (!createSwapChain(hwnd, width, height)) return false;
    if (!createDescriptorHeaps())     return false;
    if (!createRenderTargets())       return false;
    if (!createDepthStencil(width, height))    return false;
    if (!createCommandAllocators())   return false;
    if (!createRootSignature())       return false;
    if (!createPipelineState())       return false;
    if (!createVertexBuffer())        return false;
    if (!createYUVTextures(width, height))     return false;

    // Fence for GPU sync
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;
    for (auto& v : m_fenceValues) v = 0;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // Create command list (closed after creation)
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      m_cmdAllocators[0].Get(), nullptr,
                                      IID_PPV_ARGS(&m_cmdList));
    if (FAILED(hr)) return false;
    m_cmdList->Close();

    m_initialized = true;
    std::cout << "[DX12] Initialized (" << width << "x" << height << ")\n";
    return true;
}

// ── Create Device ─────────────────────────────────────────────────────────────
bool DX12Renderer::createDevice() {
    UINT dxgiFlags = 0;

#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
        dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) return false;

    // Pick best adapter (highest VRAM)
    ComPtr<IDXGIAdapter1> adapter;
    SIZE_T bestVRAM = 0;
    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                         _uuidof(ID3D12Device), nullptr))) {
            if (desc.DedicatedVideoMemory > bestVRAM) {
                bestVRAM  = desc.DedicatedVideoMemory;
                m_adapter = adapter;
            }
        }
    }

    if (!m_adapter) {
        // Fallback: WARP
        m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter));
    }

    hr = D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                            IID_PPV_ARGS(&m_device));
    return SUCCEEDED(hr);
}

// ── Create Command Queue ──────────────────────────────────────────────────────
bool DX12Renderer::createCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    return SUCCEEDED(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmdQueue)));
}

// ── Create Swap Chain ─────────────────────────────────────────────────────────
bool DX12Renderer::createSwapChain(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width       = static_cast<UINT>(width);
    desc.Height      = static_cast<UINT>(height);
    desc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc  = {1, 0};
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = k_FrameCount;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = m_factory->CreateSwapChainForHwnd(
        m_cmdQueue.Get(), hwnd, &desc, nullptr, nullptr, &sc1);
    if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);
    if (FAILED(hr)) return false;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return true;
}

// ── Create Descriptor Heaps ───────────────────────────────────────────────────
bool DX12Renderer::createDescriptorHeaps() {
    // RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = k_RtvHeapSize;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;
    m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // SRV/CBV/UAV heap (shader visible)
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = k_SrvHeapSize;
    srvDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvHeap))))
        return false;
    m_srvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = 1;
    return SUCCEEDED(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap)));
}

// ── Create Render Targets ─────────────────────────────────────────────────────
bool DX12Renderer::createRenderTargets() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < k_FrameCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescSize;
    }
    return true;
}

// ── Create Depth Stencil ──────────────────────────────────────────────────────
bool DX12Renderer::createDepthStencil(int width, int height) {
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width            = static_cast<UINT64>(width);
    rd.Height           = static_cast<UINT>(height);
    rd.DepthOrArraySize = 1;
    rd.MipLevels        = 1;
    rd.Format           = DXGI_FORMAT_D32_FLOAT;
    rd.SampleDesc       = {1, 0};
    rd.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE cv = {};
    cv.Format            = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;

    return SUCCEEDED(m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
        IID_PPV_ARGS(&m_depthStencil)));
}

// ── Create Root Signature ─────────────────────────────────────────────────────
bool DX12Renderer::createRootSignature() {
    // Descriptor table: 2 SRVs (Y + UV) + 1 sampler
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors                    = 2;
    ranges[0].BaseShaderRegister                = 0;
    ranges[0].RegisterSpace                     = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    ranges[1].NumDescriptors                    = 1;
    ranges[1].BaseShaderRegister                = 0;
    ranges[1].RegisterSpace                     = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[2] = {};
    params[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges   = &ranges[0];
    params[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    params[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges   = &ranges[1];
    params[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.NumParameters     = 2;
    rsd.pParameters       = params;
    rsd.Flags             =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS        |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS      |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &sig, &err);
    if (FAILED(hr)) return false;

    return SUCCEEDED(m_device->CreateRootSignature(
        0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

// ── Create Pipeline State ─────────────────────────────────────────────────────
bool DX12Renderer::createPipelineState() {
    ComPtr<ID3DBlob> vs, ps, err;

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    if (FAILED(D3DCompile(k_HLSL, strlen(k_HLSL), nullptr, nullptr, nullptr,
                           "VSMain", "vs_5_0", compileFlags, 0, &vs, &err))) {
        if (err) std::cerr << "[DX12] VS compile error: "
                            << (char*)err->GetBufferPointer() << "\n";
        return false;
    }

    if (FAILED(D3DCompile(k_HLSL, strlen(k_HLSL), nullptr, nullptr, nullptr,
                           "PSMain", "ps_5_0", compileFlags, 0, &ps, &err))) {
        if (err) std::cerr << "[DX12] PS compile error: "
                            << (char*)err->GetBufferPointer() << "\n";
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputElems[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_rootSignature.Get();
    psoDesc.VS                    = {vs->GetBufferPointer(), vs->GetBufferSize()};
    psoDesc.PS                    = {ps->GetBufferPointer(), ps->GetBufferSize()};
    psoDesc.InputLayout           = {inputElems, 2};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc            = {1, 0};
    psoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;

    return SUCCEEDED(m_device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

// ── Create Command Allocators ─────────────────────────────────────────────────
bool DX12Renderer::createCommandAllocators() {
    for (UINT i = 0; i < k_FrameCount; ++i) {
        if (FAILED(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_cmdAllocators[i]))))
            return false;
    }
    return true;
}

// ── Create Vertex Buffer ──────────────────────────────────────────────────────
bool DX12Renderer::createVertexBuffer() {
    const UINT vbSize = sizeof(k_Quad);

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width     = vbSize;
    rd.Height    = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc = {1, 0};
    rd.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer))))
        return false;

    void* mapped;
    m_vertexBuffer->Map(0, nullptr, &mapped);
    memcpy(mapped, k_Quad, vbSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.SizeInBytes    = vbSize;
    m_vbView.StrideInBytes  = sizeof(QuadVertex);
    return true;
}

// ── Create YUV Textures ───────────────────────────────────────────────────────
bool DX12Renderer::createYUVTextures(int width, int height) {
    // Y plane (R8)
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    auto makeTexture = [&](ComPtr<ID3D12Resource>& tex,
                           DXGI_FORMAT fmt,
                           UINT w, UINT h) -> bool {
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width            = w;
        rd.Height           = h;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = fmt;
        rd.SampleDesc       = {1, 0};
        rd.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;
        return SUCCEEDED(m_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&tex)));
    };

    if (!makeTexture(m_texY,  DXGI_FORMAT_R8_UNORM,  width, height))       return false;
    if (!makeTexture(m_texUV, DXGI_FORMAT_R8G8_UNORM, width/2, height/2))  return false;

    // Create SRVs
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvY = {};
    srvY.Format                    = DXGI_FORMAT_R8_UNORM;
    srvY.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvY.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvY.Texture2D.MipLevels       = 1;
    m_device->CreateShaderResourceView(m_texY.Get(), &srvY, h);
    h.ptr += m_srvDescSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvUV = {};
    srvUV.Format                    = DXGI_FORMAT_R8G8_UNORM;
    srvUV.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvUV.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvUV.Texture2D.MipLevels       = 1;
    m_device->CreateShaderResourceView(m_texUV.Get(), &srvUV, h);

    return true;
}

// ── Render Frame ──────────────────────────────────────────────────────────────
bool DX12Renderer::renderFrame(const VideoFrame& frame) {
    if (!m_initialized) return false;

    uploadYUVFrame(frame);
    recordCommandList(m_frameIndex);
    submitAndPresent(m_frameIndex);
    moveToNextFrame();
    return true;
}

void DX12Renderer::uploadYUVFrame(const VideoFrame& frame) {
    // TODO: upload Y + UV planes to m_texY / m_texUV via upload buffer
    // Using UpdateSubresources helper or manual copy
    (void)frame;
}

void DX12Renderer::recordCommandList(UINT frameIdx) {
    auto& allocator = m_cmdAllocators[frameIdx];
    allocator->Reset();
    m_cmdList->Reset(allocator.Get(), m_pipelineState.Get());

    // Transition back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_renderTargets[frameIdx].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_cmdList->ResourceBarrier(1, &barrier);

    // Set render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += frameIdx * m_rtvDescSize;

    const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    m_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // Viewport & scissor
    D3D12_VIEWPORT vp = {0, 0, (float)m_width, (float)m_height, 0, 1};
    D3D12_RECT     sr = {0, 0, m_width, m_height};
    m_cmdList->RSSetViewports(1, &vp);
    m_cmdList->RSSetScissorRects(1, &sr);

    // Draw
    m_cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.Get()};
    m_cmdList->SetDescriptorHeaps(1, heaps);
    m_cmdList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_cmdList->IASetVertexBuffers(0, 1, &m_vbView);
    m_cmdList->DrawInstanced(4, 1, 0, 0);

    // Transition to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    m_cmdList->ResourceBarrier(1, &barrier);

    m_cmdList->Close();
}

void DX12Renderer::submitAndPresent(UINT /*frameIdx*/) {
    ID3D12CommandList* lists[] = {m_cmdList.Get()};
    m_cmdQueue->ExecuteCommandLists(1, lists);
    m_swapChain->Present(m_vsync ? 1 : 0, m_vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);
}

void DX12Renderer::moveToNextFrame() {
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    m_cmdQueue->Signal(m_fence.Get(), currentFenceValue);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

// ── Resize ────────────────────────────────────────────────────────────────────
void DX12Renderer::resize(int width, int height) {
    if (!m_initialized) return;
    waitForGPU();

    for (auto& rt : m_renderTargets) rt.Reset();

    m_swapChain->ResizeBuffers(k_FrameCount, width, height,
                                DXGI_FORMAT_R8G8B8A8_UNORM,
                                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    m_width  = width;
    m_height = height;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    createRenderTargets();
    createDepthStencil(width, height);
    createYUVTextures(width, height);
}

// ── HDR mode ──────────────────────────────────────────────────────────────────
void DX12Renderer::setHDRMode(bool enabled) {
    m_hdrMode = enabled;
    if (enabled) enableHDROutput();
}

void DX12Renderer::enableHDROutput() {
    ComPtr<IDXGIOutput> output;
    if (FAILED(m_swapChain->GetContainingOutput(&output))) return;

    ComPtr<IDXGIOutput6> output6;
    if (FAILED(output.As(&output6))) return;

    DXGI_OUTPUT_DESC1 desc;
    output6->GetDesc1(&desc);

    if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        m_swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
        std::cout << "[DX12] HDR10 output enabled\n";
    }
}

// ── GPU sync ──────────────────────────────────────────────────────────────────
void DX12Renderer::waitForGPU() {
    m_cmdQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]);
    m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    ++m_fenceValues[m_frameIndex];
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void DX12Renderer::shutdown() {
    if (!m_initialized) return;
    waitForGPU();
    CloseHandle(m_fenceEvent);
    m_initialized = false;
    std::cout << "[DX12] Shutdown\n";
}

// ── Frame resources ───────────────────────────────────────────────────────────
bool DX12Renderer::createFrameResources() { return true; } // Handled per-field above

} // namespace aurora::core

#endif // _WIN32

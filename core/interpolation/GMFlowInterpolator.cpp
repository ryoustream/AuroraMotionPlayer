#include "GMFlowInterpolator.h"
#include "AuroraFlow.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#endif
#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

namespace aurora::interpolation {

struct GMFlowInterpolator::Impl {
#ifdef AURORA_NCNN
    ncnn::Net flowNet;   // GMFlow: computes optical flow
    ncnn::Net synthNet;  // Synthesis: warps + blends frames
#endif
#ifdef AURORA_ONNX
    std::unique_ptr<Ort::Session> flowSession;
    std::unique_ptr<Ort::Session> synthSession;
    Ort::Env            ortEnv{ORT_LOGGING_LEVEL_WARNING, "gmflow"};
    Ort::SessionOptions sessionOpts;
#endif
    InferenceBackend backend = InferenceBackend::NCNN;
};

GMFlowInterpolator::GMFlowInterpolator() : m_impl(std::make_unique<Impl>()) {}
GMFlowInterpolator::~GMFlowInterpolator() { shutdown(); }

bool GMFlowInterpolator::init(const InterpolationConfig& cfg) {
    m_impl->backend = cfg.backend;

#ifdef AURORA_NCNN
    if (cfg.backend == InferenceBackend::NCNN) {
        m_impl->flowNet.opt.use_vulkan_compute  = true;
        m_impl->synthNet.opt.use_vulkan_compute = true;
        m_impl->flowNet.opt.num_threads  = 4;
        m_impl->synthNet.opt.num_threads = 4;

        // GMFlow uses two model files: flow estimation + frame synthesis
        if (m_impl->flowNet.load_param( (cfg.modelPath+"/gmflow.param").c_str())  != 0) return false;
        if (m_impl->flowNet.load_model( (cfg.modelPath+"/gmflow.bin").c_str())    != 0) return false;
        if (m_impl->synthNet.load_param((cfg.modelPath+"/gmsynth.param").c_str()) != 0) return false;
        if (m_impl->synthNet.load_model((cfg.modelPath+"/gmsynth.bin").c_str())   != 0) return false;

        m_initialized = true;
        return true;
    }
#endif
#ifdef AURORA_ONNX
    if (cfg.backend == InferenceBackend::ONNX) {
        m_impl->sessionOpts.SetIntraOpNumThreads(4);
        m_impl->sessionOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        OrtCUDAProviderOptions cudaOpts{};
        cudaOpts.device_id = cfg.gpuDeviceId;
        m_impl->sessionOpts.AppendExecutionProvider_CUDA(cudaOpts);

        m_impl->flowSession = std::make_unique<Ort::Session>(
            m_impl->ortEnv, (cfg.modelPath+"/gmflow.onnx").c_str(), m_impl->sessionOpts);
        m_impl->synthSession = std::make_unique<Ort::Session>(
            m_impl->ortEnv, (cfg.modelPath+"/gmsynth.onnx").c_str(), m_impl->sessionOpts);

        m_initialized = true;
        return true;
    }
#endif
    return false;
}

void GMFlowInterpolator::shutdown() {
#ifdef AURORA_NCNN
    m_impl->flowNet.clear();
    m_impl->synthNet.clear();
#endif
    m_initialized = false;
}

// ── helpers ──────────────────────────────────────────────────────────────────
static int gmPad(int v) { return ((v + 31) / 32) * 32; }

static void gmYUVtoRGB(video::VideoFramePtr f, std::vector<float>& out,
                        int& outW, int& outH)
{
    int w = f->width(), h = f->height();
    outW = gmPad(w); outH = gmPad(h);
    out.assign(outW * outH * 3, 0.f);
    const uint8_t* Y  = f->data(0);
    const uint8_t* Cb = f->data(1);
    const uint8_t* Cr = f->data(2);
    int ls_y = f->linesize(0), ls_cb = f->linesize(1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float yv  = (Y[y*ls_y+x]          - 16.f)  / 219.f;
            float cbv = (Cb[(y/2)*ls_cb+(x/2)] - 128.f) / 224.f;
            float crv = (Cr[(y/2)*ls_cb+(x/2)] - 128.f) / 224.f;
            float r = std::clamp(yv + 1.5748f*crv,               0.f,1.f);
            float g = std::clamp(yv - 0.1873f*cbv - 0.4681f*crv, 0.f,1.f);
            float b = std::clamp(yv + 1.8556f*cbv,               0.f,1.f);
            int idx = (y*outW+x)*3;
            out[idx]=r; out[idx+1]=g; out[idx+2]=b;
        }
}

// Bilinear warp: warp src by flow field (2-channel HWC float)
static std::vector<float> bilinearWarp(const std::vector<float>& src,
                                        const std::vector<float>& flow,
                                        int W, int H)
{
    std::vector<float> dst(W * H * 3, 0.f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int fi = (y*W+x)*2;
            float sx = x + flow[fi];
            float sy = y + flow[fi+1];
            int x0 = (int)sx, y0 = (int)sy;
            int x1 = x0+1,    y1 = y0+1;
            float wx = sx-x0, wy = sy-y0;
            for (int c = 0; c < 3; ++c) {
                auto s = [&](int px,int py) -> float {
                    px = std::clamp(px,0,W-1);
                    py = std::clamp(py,0,H-1);
                    return src[(py*W+px)*3+c];
                };
                dst[(y*W+x)*3+c] =
                    s(x0,y0)*(1-wx)*(1-wy) + s(x1,y0)*wx*(1-wy) +
                    s(x0,y1)*(1-wx)*wy     + s(x1,y1)*wx*wy;
            }
        }
    return dst;
}

static video::VideoFramePtr gmRGBtoFrame(const std::vector<float>& rgb,
                                          int w, int h, int padW,
                                          const video::VideoFrame& ref)
{
    auto out = std::make_shared<video::VideoFrame>(w, h, video::PixelFormat::YUV420P);
    uint8_t* Y  = out->data(0);
    uint8_t* Cb = out->data(1);
    uint8_t* Cr = out->data(2);
    int ls_y = out->linesize(0), ls_cb = out->linesize(1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int idx = (y*padW+x)*3;
            float r=rgb[idx], g=rgb[idx+1], b=rgb[idx+2];
            float yv  =  0.2126f*r + 0.7152f*g + 0.0722f*b;
            float cbv = -0.1146f*r - 0.3854f*g + 0.5000f*b;
            float crv =  0.5000f*r - 0.4542f*g - 0.0458f*b;
            Y[y*ls_y+x] = (uint8_t)std::clamp(yv*219.f+16.f,0.f,255.f);
            if (!(y%2) && !(x%2)) {
                Cb[(y/2)*ls_cb+(x/2)] = (uint8_t)std::clamp(cbv*224.f+128.f,0.f,255.f);
                Cr[(y/2)*ls_cb+(x/2)] = (uint8_t)std::clamp(crv*224.f+128.f,0.f,255.f);
            }
        }
    out->setTimeBase(ref.timeBase());
    out->setColorMeta(ref.colorMeta());
    return out;
}

// ── interpolate ──────────────────────────────────────────────────────────────
// GMFlow pipeline:
//   1. flowNet(img0, img1) → flow_01, flow_10   (optical flow both directions)
//   2. Warp img0 by t*flow_01, warp img1 by (1-t)*flow_10
//   3. synthNet(warp0, warp1, t) → blended output
video::VideoFramePtr GMFlowInterpolator::interpolate(
    video::VideoFramePtr f0, video::VideoFramePtr f1, float t)
{
    if (!m_initialized || !f0 || !f1) return nullptr;

    int padW, padH;
    std::vector<float> rgb0, rgb1;
    gmYUVtoRGB(f0, rgb0, padW, padH);
    gmYUVtoRGB(f1, rgb1, padW, padH);
    int N = padW * padH;

#ifdef AURORA_NCNN
    if (m_impl->backend == InferenceBackend::NCNN) {
        // Step 1: flow estimation
        ncnn::Mat in0(padW,padH,3), in1(padW,padH,3);
        memcpy(in0.data, rgb0.data(), N*3*sizeof(float));
        memcpy(in1.data, rgb1.data(), N*3*sizeof(float));

        ncnn::Extractor flowEx = m_impl->flowNet.create_extractor();
        flowEx.input("img0", in0);
        flowEx.input("img1", in1);

        ncnn::Mat flow01, flow10;
        if (flowEx.extract("flow_01", flow01) != 0) return nullptr;
        if (flowEx.extract("flow_10", flow10) != 0) return nullptr;

        // flow mats: shape (padW, padH, 2)
        std::vector<float> fwd(N*2), bwd(N*2);
        memcpy(fwd.data(), flow01.data, N*2*sizeof(float));
        memcpy(bwd.data(), flow10.data, N*2*sizeof(float));

        // Scale flows by t / (1-t)
        for (int i=0;i<N*2;++i) { fwd[i]*=t; bwd[i]*=(1.f-t); }

        // Step 2: warp
        auto w0 = bilinearWarp(rgb0, fwd, padW, padH);
        auto w1 = bilinearWarp(rgb1, bwd, padW, padH);

        // Step 3: synthesis
        ncnn::Mat mw0(padW,padH,3), mw1(padW,padH,3);
        memcpy(mw0.data, w0.data(), N*3*sizeof(float));
        memcpy(mw1.data, w1.data(), N*3*sizeof(float));
        ncnn::Mat ts(1); ((float*)ts.data)[0] = t;

        ncnn::Extractor synthEx = m_impl->synthNet.create_extractor();
        synthEx.input("warp0", mw0);
        synthEx.input("warp1", mw1);
        synthEx.input("t", ts);

        ncnn::Mat synthOut;
        if (synthEx.extract("output", synthOut) != 0) return nullptr;

        std::vector<float> outRGB(N*3);
        memcpy(outRGB.data(), synthOut.data, N*3*sizeof(float));
        return gmRGBtoFrame(outRGB, f0->width(), f0->height(), padW, *f0);
    }
#endif
#ifdef AURORA_ONNX
    if (m_impl->backend == InferenceBackend::ONNX) {
        std::vector<float> nchw0(3*N), nchw1(3*N);
        for (int c=0;c<3;++c) for (int i=0;i<N;++i) {
            nchw0[c*N+i]=rgb0[i*3+c];
            nchw1[c*N+i]=rgb1[i*3+c];
        }
        std::vector<int64_t> shp={1,3,padH,padW};
        auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
        std::array<Ort::Value,2> flowIns = {
            Ort::Value::CreateTensor<float>(mem,nchw0.data(),3*N,shp.data(),4),
            Ort::Value::CreateTensor<float>(mem,nchw1.data(),3*N,shp.data(),4)
        };
        const char* fInames[]={"img0","img1"};
        const char* fOnames[]={"flow_01","flow_10"};
        auto flowOuts = m_impl->flowSession->Run(
            Ort::RunOptions{nullptr},fInames,flowIns.data(),2,fOnames,2);

        float* fwd_p = flowOuts[0].GetTensorMutableData<float>();
        float* bwd_p = flowOuts[1].GetTensorMutableData<float>();

        // Convert NCHW flow (1,2,H,W) → HWC
        std::vector<float> fwd(N*2), bwd(N*2);
        for (int i=0;i<N;++i) {
            fwd[i*2]   = fwd_p[i]   * t;
            fwd[i*2+1] = fwd_p[N+i] * t;
            bwd[i*2]   = bwd_p[i]   * (1.f-t);
            bwd[i*2+1] = bwd_p[N+i] * (1.f-t);
        }

        auto w0 = bilinearWarp(rgb0, fwd, padW, padH);
        auto w1 = bilinearWarp(rgb1, bwd, padW, padH);

        // NCHW for synthesis
        std::vector<float> sw0(3*N), sw1(3*N);
        for (int c=0;c<3;++c) for (int i=0;i<N;++i) {
            sw0[c*N+i]=w0[i*3+c];
            sw1[c*N+i]=w1[i*3+c];
        }
        std::array<Ort::Value,3> sIns = {
            Ort::Value::CreateTensor<float>(mem,sw0.data(),3*N,shp.data(),4),
            Ort::Value::CreateTensor<float>(mem,sw1.data(),3*N,shp.data(),4),
            Ort::Value::CreateTensor<float>(mem,&t,1,std::array<int64_t,1>{1}.data(),1)
        };
        const char* sInames[]={"warp0","warp1","t"};
        const char* sOnames[]={"output"};
        auto sOuts = m_impl->synthSession->Run(
            Ort::RunOptions{nullptr},sInames,sIns.data(),3,sOnames,1);

        float* rd = sOuts[0].GetTensorMutableData<float>();
        std::vector<float> hwc(N*3);
        for (int c=0;c<3;++c) for (int i=0;i<N;++i) hwc[i*3+c]=rd[c*N+i];
        return gmRGBtoFrame(hwc, f0->width(), f0->height(), padW, *f0);
    }
#endif
    return nullptr;
}

} // namespace aurora::interpolation

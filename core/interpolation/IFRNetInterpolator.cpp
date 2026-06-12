#include "IFRNetInterpolator.h"
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

struct IFRNetInterpolator::Impl {
#ifdef AURORA_NCNN
    ncnn::Net net;
#endif
#ifdef AURORA_ONNX
    std::unique_ptr<Ort::Session> session;
    Ort::Env    ortEnv{ORT_LOGGING_LEVEL_WARNING, "ifrnet"};
    Ort::SessionOptions sessionOpts;
#endif
    InferenceBackend backend = InferenceBackend::NCNN;
    int  tileSize  = 0;
    bool useTTA    = false;
};

IFRNetInterpolator::IFRNetInterpolator()
    : m_impl(std::make_unique<Impl>()) {}

IFRNetInterpolator::~IFRNetInterpolator() { shutdown(); }

bool IFRNetInterpolator::init(const InterpolationConfig& cfg) {
    m_impl->backend  = cfg.backend;
    m_impl->tileSize = cfg.tileSize;
    m_impl->useTTA   = cfg.useTTA;

    switch (cfg.quality) {
    case InterpolationQuality::Fast:     m_impl->tileSize = 192; break;
    case InterpolationQuality::Balanced: m_impl->tileSize = 0;   break;
    case InterpolationQuality::High:
    case InterpolationQuality::Ultra:    m_impl->tileSize = 0;   break;
    }

#ifdef AURORA_NCNN
    if (cfg.backend == InferenceBackend::NCNN) {
        m_impl->net.opt.use_vulkan_compute = true;
        m_impl->net.opt.num_threads = 4;
        if (m_impl->net.load_param((cfg.modelPath + "/ifrnet.param").c_str()) != 0) return false;
        if (m_impl->net.load_model((cfg.modelPath + "/ifrnet.bin").c_str())   != 0) return false;
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
        m_impl->session = std::make_unique<Ort::Session>(
            m_impl->ortEnv,
            (cfg.modelPath + "/ifrnet.onnx").c_str(),
            m_impl->sessionOpts);
        m_initialized = true;
        return true;
    }
#endif
    return false;
}

void IFRNetInterpolator::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
    m_initialized = false;
}

// ── helpers (same BT.709 conversion as RIFE) ────────────────────────────────
static int padTo(int v, int align) { return ((v + align - 1) / align) * align; }

static void yuvToRGBFloat(video::VideoFramePtr f,
                           std::vector<float>& out, int& outW, int& outH)
{
    int w = f->width(), h = f->height();
    outW = padTo(w, 32); outH = padTo(h, 32);
    out.assign(outW * outH * 3, 0.0f);
    const uint8_t* Y  = f->data(0);
    const uint8_t* Cb = f->data(1);
    const uint8_t* Cr = f->data(2);
    int ls_y = f->linesize(0), ls_cb = f->linesize(1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float yv  = (Y[y*ls_y+x]           - 16.f)  / 219.f;
            float cbv = (Cb[(y/2)*ls_cb+(x/2)]  - 128.f) / 224.f;
            float crv = (Cr[(y/2)*ls_cb+(x/2)]  - 128.f) / 224.f;
            float r = std::clamp(yv + 1.5748f*crv,              0.f,1.f);
            float g = std::clamp(yv - 0.1873f*cbv - 0.4681f*crv,0.f,1.f);
            float b = std::clamp(yv + 1.8556f*cbv,              0.f,1.f);
            int idx = (y*outW+x)*3;
            out[idx]=r; out[idx+1]=g; out[idx+2]=b;
        }
}

static video::VideoFramePtr rgbFloatToFrame(const std::vector<float>& rgb,
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
            Y[y*ls_y+x] = (uint8_t)std::clamp(yv*219.f+16.f, 0.f,255.f);
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
// IFRNet I/O: inputs "I0","I1","t_value" → output "I_t"
video::VideoFramePtr IFRNetInterpolator::interpolate(
    video::VideoFramePtr f0, video::VideoFramePtr f1, float t)
{
    if (!m_initialized || !f0 || !f1) return nullptr;

    int padW, padH;
    std::vector<float> rgb0, rgb1;
    yuvToRGBFloat(f0, rgb0, padW, padH);
    yuvToRGBFloat(f1, rgb1, padW, padH);

#ifdef AURORA_NCNN
    if (m_impl->backend == InferenceBackend::NCNN) {
        ncnn::Mat in0(padW, padH, 3), in1(padW, padH, 3);
        memcpy(in0.data, rgb0.data(), rgb0.size()*sizeof(float));
        memcpy(in1.data, rgb1.data(), rgb1.size()*sizeof(float));
        ncnn::Mat ts(1); ((float*)ts.data)[0] = t;

        ncnn::Extractor ex = m_impl->net.create_extractor();
        ex.input("I0", in0);
        ex.input("I1", in1);
        ex.input("t_value", ts);

        ncnn::Mat out;
        if (ex.extract("I_t", out) != 0) return nullptr;

        std::vector<float> outRGB(padW*padH*3);
        memcpy(outRGB.data(), out.data, outRGB.size()*sizeof(float));
        return rgbFloatToFrame(outRGB, f0->width(), f0->height(), padW, *f0);
    }
#endif
#ifdef AURORA_ONNX
    if (m_impl->backend == InferenceBackend::ONNX) {
        // NCHW layout
        int N = padH*padW;
        std::vector<float> nchw0(3*N), nchw1(3*N);
        for (int c=0;c<3;++c) for (int i=0;i<N;++i) {
            nchw0[c*N+i]=rgb0[i*3+c];
            nchw1[c*N+i]=rgb1[i*3+c];
        }
        std::vector<int64_t> shp = {1,3,padH,padW};
        auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
        std::array<Ort::Value,3> ins = {
            Ort::Value::CreateTensor<float>(mem,nchw0.data(),nchw0.size(),shp.data(),4),
            Ort::Value::CreateTensor<float>(mem,nchw1.data(),nchw1.size(),shp.data(),4),
            Ort::Value::CreateTensor<float>(mem,&t,1,std::array<int64_t,1>{1}.data(),1)
        };
        const char* inames[]={"I0","I1","t_value"};
        const char* onames[]={"I_t"};
        auto outs = m_impl->session->Run(Ort::RunOptions{nullptr},inames,ins.data(),3,onames,1);
        float* rd = outs[0].GetTensorMutableData<float>();
        // NCHW → HWC
        std::vector<float> hwc(N*3);
        for (int c=0;c<3;++c) for (int i=0;i<N;++i) hwc[i*3+c]=rd[c*N+i];
        return rgbFloatToFrame(hwc, f0->width(), f0->height(), padW, *f0);
    }
#endif
    return nullptr;
}

} // namespace aurora::interpolation

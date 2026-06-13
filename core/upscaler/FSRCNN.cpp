// ─────────────────────────────────────────────────────────────────────────────
// FSRCNN.cpp  —  Aurora Motion Player
// FSRCNN (Fast Super-Resolution Convolutional Neural Network) inference.
// Reference: Dong et al., "Accelerating the Super-Resolution CNN", ECCV 2016.
// Faster and lighter than ESRGAN; suitable for real-time CPU/low-power GPU.
// Supports ONNX Runtime and NCNN backends.
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerFactory.h"
#include "ImageUtils.h"
#include <algorithm>
#include <cmath>

#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#  include <ncnn/mat.h>
#endif

namespace aurora::upscaler {

struct FSRCNN::Impl {
#ifdef AURORA_ONNX
    Ort::Env                      env{ORT_LOGGING_LEVEL_WARNING, "FSRCNN"};
    Ort::SessionOptions           sessionOpts;
    std::unique_ptr<Ort::Session> session;
    std::vector<const char*>      inputNames  = {"input"};
    std::vector<const char*>      outputNames = {"output"};
#endif
#ifdef AURORA_NCNN
    ncnn::Net net;
#endif
    UpscalerConfig cfg;
};

FSRCNN::FSRCNN()  : m_impl(std::make_unique<Impl>()) {}
FSRCNN::~FSRCNN() { shutdown(); }

bool FSRCNN::init(const UpscalerConfig& cfg) {
    m_impl->cfg = cfg;

#ifdef AURORA_ONNX
    m_impl->sessionOpts.SetIntraOpNumThreads(4);
    m_impl->sessionOpts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

    // Try CUDA EP first, fall back to CPU
    try {
        OrtCUDAProviderOptions cuda{};
        cuda.device_id = cfg.gpuDeviceId;
        m_impl->sessionOpts.AppendExecutionProvider_CUDA(cuda);
    } catch (...) { /* CPU fallback */ }

    std::wstring path(cfg.modelPath.begin(), cfg.modelPath.end());
    path += L"/fsrcnn-x" + std::to_wstring(static_cast<int>(cfg.factor)) + L".onnx";

    m_impl->session = std::make_unique<Ort::Session>(
        m_impl->env, path.c_str(), m_impl->sessionOpts);

    m_initialized = true;
    return true;

#elif defined(AURORA_NCNN)
    m_impl->net.opt.use_vulkan_compute = false; // FSRCNN typically runs on CPU
    m_impl->net.opt.num_threads        = 4;

    std::string scale = std::to_string(static_cast<int>(cfg.factor));
    std::string param = cfg.modelPath + "/fsrcnn-x" + scale + ".param";
    std::string bin   = cfg.modelPath + "/fsrcnn-x" + scale + ".bin";

    if (m_impl->net.load_param(param.c_str()) != 0) return false;
    if (m_impl->net.load_model(bin.c_str())   != 0) return false;

    m_initialized = true;
    return true;
#else
    (void)cfg;
    m_initialized = true; // CPU bilinear fallback
    return true;
#endif
}

void FSRCNN::shutdown() {
#ifdef AURORA_ONNX
    m_impl->session.reset();
#endif
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
    m_initialized = false;
}

// FSRCNN operates on the Y channel (luma) only — standard SR practice
static std::vector<float> extractLuma(const std::vector<float>& rgb, int w, int h) {
    std::vector<float> luma(w * h);
    for (int i = 0; i < w * h; ++i)
        luma[i] = 0.2126f * rgb[i*3+0] + 0.7152f * rgb[i*3+1] + 0.0722f * rgb[i*3+2];
    return luma;
}

static void applyLuma(const std::vector<float>& origRgb,
                       const std::vector<float>& upLuma,
                       int origW, int origH,
                       int outW, int outH,
                       int scale,
                       std::vector<float>& outRgb) {
    // Upscale Cb/Cr with bilinear, replace Y with SR luma
    // Pack Cb, Cr
    std::vector<float> cbcr(origW * origH * 2);
    for (int i = 0; i < origW * origH; ++i) {
        // Rough RGB → YCbCr
        float R = origRgb[i*3+0], G = origRgb[i*3+1], B = origRgb[i*3+2];
        float Cb = (B - (0.2126f * R + 0.7152f * G + 0.0722f * B)) / 1.85563f;
        float Cr = (R - (0.2126f * R + 0.7152f * G + 0.0722f * B)) / 1.57480f;
        cbcr[i*2+0] = Cb + 0.5f;
        cbcr[i*2+1] = Cr + 0.5f;
    }
    auto upCbCr = bilinearResize(cbcr, origW, origH, 2, outW, outH);

    outRgb.resize(outW * outH * 3);
    for (int i = 0; i < outW * outH; ++i) {
        float Y  = upLuma[i];
        float Cb = upCbCr[i*2+0] - 0.5f;
        float Cr = upCbCr[i*2+1] - 0.5f;
        outRgb[i*3+0] = std::clamp(Y + 1.57480f * Cr, 0.f, 1.f);
        outRgb[i*3+1] = std::clamp(Y - 0.18733f * Cb - 0.46813f * Cr, 0.f, 1.f);
        outRgb[i*3+2] = std::clamp(Y + 1.85563f * Cb, 0.f, 1.f);
    }
}

video::VideoFramePtr FSRCNN::process(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return frame;

    const int scale = static_cast<int>(m_impl->cfg.factor);
    const int srcW  = frame->width();
    const int srcH  = frame->height();
    const int outW  = srcW * scale;
    const int outH  = srcH * scale;

    auto rgb  = frameToRGB(*frame);
    auto luma = extractLuma(rgb, srcW, srcH);
    std::vector<float> upLuma(outW * outH, 0.f);

#ifdef AURORA_ONNX
    // FSRCNN input: NCHW with 1 channel (luma)
    std::array<int64_t, 4> shape{1, 1, srcH, srcW};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, luma.data(), luma.size(), shape.data(), shape.size());

    auto outputs = m_impl->session->Run(
        Ort::RunOptions{}, m_impl->inputNames.data(), &inputTensor, 1,
        m_impl->outputNames.data(), 1);

    float* outData = outputs[0].GetTensorMutableData<float>();
    upLuma.assign(outData, outData + outW * outH);
    for (auto& v : upLuma) v = std::clamp(v, 0.f, 1.f);

#elif defined(AURORA_NCNN)
    ncnn::Mat in(srcW, srcH, 1);
    std::memcpy(in.data, luma.data(), luma.size() * sizeof(float));

    ncnn::Extractor ex = m_impl->net.create_extractor();
    ex.input("input", in);
    ncnn::Mat out;
    ex.extract("output", out);

    for (int i = 0; i < outW * outH; ++i)
        upLuma[i] = std::clamp(static_cast<const float*>(out.data)[i], 0.f, 1.f);
#else
    // CPU bilinear fallback
    upLuma = bilinearResize(luma, srcW, srcH, 1, outW, outH);
#endif

    std::vector<float> outRgb;
    applyLuma(rgb, upLuma, srcW, srcH, outW, outH, scale, outRgb);

    auto out = std::make_shared<video::VideoFrame>(outW, outH, video::PixelFormat::YUV420P);
    rgbToFrame(outRgb, outW, outH, *out);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    out->setColorMeta(frame->colorMeta());
    return out;
}

} // namespace aurora::upscaler

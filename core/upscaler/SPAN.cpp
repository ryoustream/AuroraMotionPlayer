// ─────────────────────────────────────────────────────────────────────────────
// SPAN.cpp  —  Aurora Motion Player
// SPAN (Swift Parameter-free Attention Network) — lightweight super-resolution.
// Reference: https://github.com/hongyuanyu/SPAN
// Lower VRAM footprint than RealESRGAN; optimized for real-time use.
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerFactory.h"
#include "ImageUtils.h"
#include <algorithm>

#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#  include <ncnn/mat.h>
#endif

#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

namespace aurora::upscaler {

struct SPAN::Impl {
#ifdef AURORA_NCNN
    ncnn::Net  net;
#endif
#ifdef AURORA_ONNX
    Ort::Env                      env{ORT_LOGGING_LEVEL_WARNING, "SPAN"};
    Ort::SessionOptions           sessionOpts;
    std::unique_ptr<Ort::Session> session;
    std::vector<const char*>      inputNames  = {"input"};
    std::vector<const char*>      outputNames = {"output"};
#endif
    UpscalerConfig cfg;
};

SPAN::SPAN()  : m_impl(std::make_unique<Impl>()) {}
SPAN::~SPAN() { shutdown(); }

bool SPAN::init(const UpscalerConfig& cfg) {
    m_impl->cfg = cfg;

#ifdef AURORA_NCNN
    m_impl->net.opt.use_vulkan_compute = true;
    m_impl->net.opt.num_threads        = 4;

    std::string scale = std::to_string(static_cast<int>(cfg.factor));
    std::string param = cfg.modelPath + "/span-x" + scale + ".param";
    std::string bin   = cfg.modelPath + "/span-x" + scale + ".bin";

    if (m_impl->net.load_param(param.c_str()) != 0) return false;
    if (m_impl->net.load_model(bin.c_str())   != 0) return false;

    m_initialized = true;
    return true;
#elif defined(AURORA_ONNX)
    m_impl->sessionOpts.SetIntraOpNumThreads(4);
    m_impl->sessionOpts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

    OrtCUDAProviderOptions cuda{};
    cuda.device_id = cfg.gpuDeviceId;
    m_impl->sessionOpts.AppendExecutionProvider_CUDA(cuda);

    std::string scale = std::to_string(static_cast<int>(cfg.factor));
    std::wstring path(cfg.modelPath.begin(), cfg.modelPath.end());
    path += L"/span-x" + std::to_wstring(static_cast<int>(cfg.factor)) + L".onnx";

    m_impl->session = std::make_unique<Ort::Session>(
        m_impl->env, path.c_str(), m_impl->sessionOpts);

    m_initialized = true;
    return true;
#else
    (void)cfg;
    return false;
#endif
}

void SPAN::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
#ifdef AURORA_ONNX
    m_impl->session.reset();
#endif
    m_initialized = false;
}

video::VideoFramePtr SPAN::process(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return frame;

    const int scale = static_cast<int>(m_impl->cfg.factor);
    const int srcW  = frame->width();
    const int srcH  = frame->height();
    const int outW  = srcW * scale;
    const int outH  = srcH * scale;

    auto rgb = frameToRGB(*frame);
    std::vector<float> result;

#ifdef AURORA_NCNN
    // Pad to multiple of 4 (SPAN requirement)
    int padW, padH;
    auto padded = padToAlign(rgb, srcW, srcH, 3, 4, padW, padH);

    ncnn::Mat in(padW, padH, 3);
    for (int y = 0; y < padH; ++y)
        for (int x = 0; x < padW; ++x) {
            in.channel(0)[y * padW + x] = padded[(y * padW + x) * 3 + 0];
            in.channel(1)[y * padW + x] = padded[(y * padW + x) * 3 + 1];
            in.channel(2)[y * padW + x] = padded[(y * padW + x) * 3 + 2];
        }

    ncnn::Extractor ex = m_impl->net.create_extractor();
    ex.set_vulkan_compute(true);
    ex.input("input", in);

    ncnn::Mat out;
    ex.extract("output", out);

    int fullOutW = padW * scale;
    int fullOutH = padH * scale;
    std::vector<float> full(fullOutW * fullOutH * 3);
    for (int y = 0; y < fullOutH; ++y)
        for (int x = 0; x < fullOutW; ++x) {
            full[(y * fullOutW + x) * 3 + 0] = std::clamp(out.channel(0)[y * fullOutW + x], 0.f, 1.f);
            full[(y * fullOutW + x) * 3 + 1] = std::clamp(out.channel(1)[y * fullOutW + x], 0.f, 1.f);
            full[(y * fullOutW + x) * 3 + 2] = std::clamp(out.channel(2)[y * fullOutW + x], 0.f, 1.f);
        }
    result = cropBuffer(full, fullOutW, fullOutH, 3, outW, outH);

#elif defined(AURORA_ONNX)
    auto nchw = hwcToNchw(rgb, srcW, srcH);
    std::array<int64_t, 4> shape{1, 3, srcH, srcW};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, nchw.data(), nchw.size(), shape.data(), shape.size());

    auto outputs = m_impl->session->Run(
        Ort::RunOptions{}, m_impl->inputNames.data(), &inputTensor, 1,
        m_impl->outputNames.data(), 1);

    float* outData  = outputs[0].GetTensorMutableData<float>();
    std::vector<float> outNchw(outData, outData + outW * outH * 3);
    result = nchwToHwc(outNchw, outW, outH);
    for (auto& v : result) v = std::clamp(v, 0.f, 1.f);
#else
    result = bilinearResize(rgb, srcW, srcH, 3, outW, outH);
#endif

    auto out = std::make_shared<video::VideoFrame>(outW, outH, video::PixelFormat::YUV420P);
    rgbToFrame(result, outW, outH, *out);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    out->setColorMeta(frame->colorMeta());
    return out;
}

} // namespace aurora::upscaler

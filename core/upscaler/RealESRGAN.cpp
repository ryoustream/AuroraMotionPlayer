// ─────────────────────────────────────────────────────────────────────────────
// RealESRGAN.cpp  —  Aurora Motion Player
// Real-ESRGAN super-resolution via NCNN (Vulkan compute) or ONNX Runtime.
// Architecture: RRDB (Residual-in-Residual Dense Block) based ESRGAN variant.
// Supports 2×/4× models with optional tiled processing for VRAM-constrained GPUs.
// ─────────────────────────────────────────────────────────────────────────────
#include "UpscalerFactory.h"
#include "ImageUtils.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#  include <ncnn/mat.h>
#endif

#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

namespace aurora::upscaler {

struct RealESRGAN::Impl {
#ifdef AURORA_NCNN
    ncnn::Net  net;
    std::string inputName  = "input";
    std::string outputName = "output";
#endif
#ifdef AURORA_ONNX
    Ort::Env                    env{ORT_LOGGING_LEVEL_WARNING, "RealESRGAN"};
    Ort::SessionOptions         sessionOpts;
    std::unique_ptr<Ort::Session> session;
    std::vector<const char*>    inputNames  = {"input"};
    std::vector<const char*>    outputNames = {"output"};
#endif
    int  tileSize    = 256;
    int  tileOverlap = 16;
    bool useTiling   = false;
};

RealESRGAN::RealESRGAN() : m_impl(std::make_unique<Impl>()) {}
RealESRGAN::~RealESRGAN() { shutdown(); }

bool RealESRGAN::init(const UpscalerConfig& cfg) {
    m_cfg = cfg;

    m_impl->tileSize    = (cfg.tileSize > 0) ? cfg.tileSize : 256;
    m_impl->tileOverlap = 16;
    m_impl->useTiling   = (cfg.factor == UpscaleFactor::X4 ||
                            cfg.factor == UpscaleFactor::X8 ||
                            cfg.tileSize > 0);

#ifdef AURORA_NCNN
    m_impl->net.opt.use_vulkan_compute = true;
    m_impl->net.opt.num_threads        = 4;
    std::string scale = std::to_string(static_cast<int>(cfg.factor));
    std::string param = cfg.modelPath + "/realesrgan-x" + scale + ".param";
    std::string bin   = cfg.modelPath + "/realesrgan-x" + scale + ".bin";
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
    std::wstring modelPath(cfg.modelPath.begin(), cfg.modelPath.end());
    modelPath += L"/realesrgan-x" +
                 std::to_wstring(static_cast<int>(cfg.factor)) + L".onnx";
    m_impl->session = std::make_unique<Ort::Session>(
        m_impl->env, modelPath.c_str(), m_impl->sessionOpts);
    m_initialized = true;
    return true;
#else
    (void)cfg;
    return false; // No AI backend compiled
#endif
}

void RealESRGAN::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
#ifdef AURORA_ONNX
    m_impl->session.reset();
#endif
    m_initialized = false;
}

// ── Run inference on one RGB tile (HWC float 0..1) ───────────────────────────
std::vector<float> RealESRGAN::runInference(const std::vector<float>& rgb,
                                              int w, int h,
                                              int outW, int outH) const {
#ifdef AURORA_NCNN
    ncnn::Mat in(w, h, 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            in.channel(0)[y * w + x] = rgb[(y * w + x) * 3 + 0];
            in.channel(1)[y * w + x] = rgb[(y * w + x) * 3 + 1];
            in.channel(2)[y * w + x] = rgb[(y * w + x) * 3 + 2];
        }
    ncnn::Extractor ex = m_impl->net.create_extractor();
    ex.set_vulkan_compute(true);
    ex.input(m_impl->inputName.c_str(), in);
    ncnn::Mat out;
    ex.extract(m_impl->outputName.c_str(), out);
    std::vector<float> result(outW * outH * 3);
    for (int y = 0; y < outH; ++y)
        for (int x = 0; x < outW; ++x) {
            result[(y * outW + x) * 3 + 0] = std::clamp(out.channel(0)[y * outW + x], 0.f, 1.f);
            result[(y * outW + x) * 3 + 1] = std::clamp(out.channel(1)[y * outW + x], 0.f, 1.f);
            result[(y * outW + x) * 3 + 2] = std::clamp(out.channel(2)[y * outW + x], 0.f, 1.f);
        }
    return result;
#elif defined(AURORA_ONNX)
    auto nchw = hwcToNchw(rgb, w, h);
    std::array<int64_t, 4> shape{1, 3, static_cast<int64_t>(h), static_cast<int64_t>(w)};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, nchw.data(), nchw.size(), shape.data(), shape.size());
    auto outputs = m_impl->session->Run(
        Ort::RunOptions{}, m_impl->inputNames.data(), &inputTensor, 1,
        m_impl->outputNames.data(), 1);
    float* outData  = outputs[0].GetTensorMutableData<float>();
    std::vector<float> outNchw(outData, outData + outW * outH * 3);
    auto result = nchwToHwc(outNchw, outW, outH);
    for (auto& v : result) v = std::clamp(v, 0.f, 1.f);
    return result;
#else
    return bilinearResize(rgb, w, h, 3, outW, outH);
#endif
}

video::VideoFramePtr RealESRGAN::process(video::VideoFramePtr frame) {
    if (!m_initialized || !frame) return frame;

    const int scale = static_cast<int>(m_cfg.factor);
    const int srcW  = frame->width();
    const int srcH  = frame->height();
    const int outW  = srcW * scale;
    const int outH  = srcH * scale;

    auto rgb = frameToRGB(*frame);
    std::vector<float> result(outW * outH * 3, 0.f);

    if (m_impl->useTiling) {
        const int tSz  = m_impl->tileSize;
        const int tOvr = m_impl->tileOverlap;
        auto tiles = computeTiles(srcW, srcH, tSz, tOvr, 4);

        std::vector<float> accum(outW * outH * 3, 0.f);
        std::vector<float> wsum(outW * outH, 0.f);

        for (const auto& tile : tiles) {
            auto tileRgb = extractTile(rgb, srcW, srcH, 3, tile);
            int tOutW = tile.w * scale;
            int tOutH = tile.h * scale;
            auto tileOut = runInference(tileRgb, tile.w, tile.h, tOutW, tOutH);
            auto weights = buildCosineWeights(tOutW, tOutH, tOvr * scale);

            TileRect outTile = tile;
            outTile.x *= scale;  outTile.y *= scale;
            outTile.w = tOutW;   outTile.h = tOutH;
            blendTile(tileOut, weights, srcW, srcH, scale, 3, outTile, accum, wsum);
        }

        for (int i = 0; i < outW * outH; ++i) {
            float w = (wsum[i] > 1e-6f) ? wsum[i] : 1.f;
            result[i * 3 + 0] = std::clamp(accum[i * 3 + 0] / w, 0.f, 1.f);
            result[i * 3 + 1] = std::clamp(accum[i * 3 + 1] / w, 0.f, 1.f);
            result[i * 3 + 2] = std::clamp(accum[i * 3 + 2] / w, 0.f, 1.f);
        }
    } else {
        result = runInference(rgb, srcW, srcH, outW, outH);
    }

    auto out = std::make_shared<video::VideoFrame>(outW, outH, video::PixelFormat::YUV420P);
    rgbToFrame(result, outW, outH, *out);
    out->setPts(frame->pts());
    out->setTimeBase(frame->timeBase());
    out->setColorMeta(frame->colorMeta());
    return out;
}

} // namespace aurora::upscaler

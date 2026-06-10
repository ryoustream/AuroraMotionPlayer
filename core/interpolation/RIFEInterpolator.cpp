#include "RIFEInterpolator.h"
#include "AuroraFlow.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <cstring>

// NCNN conditional compile
#ifdef AURORA_NCNN
#  include <ncnn/net.h>
#endif

// ONNX Runtime conditional compile
#ifdef AURORA_ONNX
#  include <onnxruntime_cxx_api.h>
#endif

namespace aurora::interpolation {

struct RIFEInterpolator::Impl {
#ifdef AURORA_NCNN
    ncnn::Net net;
#endif
#ifdef AURORA_ONNX
    std::unique_ptr<Ort::Session> session;
    Ort::Env ortEnv{ORT_LOGGING_LEVEL_WARNING, "rife"};
    Ort::SessionOptions sessionOpts;
#endif
    InferenceBackend backend = InferenceBackend::NCNN;
    std::string modelPath;
};

RIFEInterpolator::RIFEInterpolator()
    : m_impl(std::make_unique<Impl>())
{}

RIFEInterpolator::~RIFEInterpolator() {
    shutdown();
}

bool RIFEInterpolator::init(const InterpolationConfig& cfg) {
    m_impl->backend   = cfg.backend;
    m_impl->modelPath = cfg.modelPath;
    m_useTTA          = cfg.useTTA;
    m_tileSize        = cfg.tileSize;

    // Quality → inference resolution scale
    // Fast: half-res inference; Ultra: full-res
    switch (cfg.quality) {
    case InterpolationQuality::Fast:     m_tileSize = 256; break;
    case InterpolationQuality::Balanced: m_tileSize = 0;   break;
    case InterpolationQuality::High:     m_tileSize = 0;   break;
    case InterpolationQuality::Ultra:    m_tileSize = 0;   break;
    }

#ifdef AURORA_NCNN
    if (cfg.backend == InferenceBackend::NCNN) {
        m_impl->net.opt.use_vulkan_compute = true;
        m_impl->net.opt.num_threads = 4;

        std::string paramPath = cfg.modelPath + "/rife.param";
        std::string binPath   = cfg.modelPath + "/rife.bin";

        if (m_impl->net.load_param(paramPath.c_str()) != 0) return false;
        if (m_impl->net.load_model(binPath.c_str())   != 0) return false;

        m_initialized = true;
        return true;
    }
#endif

#ifdef AURORA_ONNX
    if (cfg.backend == InferenceBackend::ONNX) {
        m_impl->sessionOpts.SetIntraOpNumThreads(4);
        m_impl->sessionOpts.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Enable CUDA EP if available
        OrtCUDAProviderOptions cudaOpts;
        cudaOpts.device_id = cfg.gpuDeviceId;
        m_impl->sessionOpts.AppendExecutionProvider_CUDA(cudaOpts);

        std::string modelFile = cfg.modelPath + "/rife.onnx";
        m_impl->session = std::make_unique<Ort::Session>(
            m_impl->ortEnv,
            modelFile.c_str(),
            m_impl->sessionOpts);

        m_initialized = true;
        return true;
    }
#endif

    // If no backend compiled, return false
    return false;
}

void RIFEInterpolator::shutdown() {
#ifdef AURORA_NCNN
    m_impl->net.clear();
#endif
    m_initialized = false;
}

int RIFEInterpolator::padTo32(int val) const noexcept {
    return ((val + 31) / 32) * 32;
}

// ── YUV420P → RGB float [0,1] ─────────────────────────────────────────────────
void RIFEInterpolator::yuvToRGBFloat(video::VideoFramePtr frame,
                                      std::vector<float>& out,
                                      int& outW, int& outH)
{
    int w = frame->width(), h = frame->height();
    outW = padTo32(w);
    outH = padTo32(h);

    out.assign(outW * outH * 3, 0.0f);

    const uint8_t* Y  = frame->data(0);
    const uint8_t* Cb = frame->data(1);
    const uint8_t* Cr = frame->data(2);
    int ls_y  = frame->linesize(0);
    int ls_cb = frame->linesize(1);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float yv  = (Y[y * ls_y + x]               - 16.0f) / 219.0f;
            float cbv = (Cb[(y/2) * ls_cb + (x/2)]     - 128.0f) / 224.0f;
            float crv = (Cr[(y/2) * ls_cb + (x/2)]     - 128.0f) / 224.0f;

            // BT.709 YCbCr → RGB
            float r = std::clamp(yv + 1.5748f * crv,              0.0f, 1.0f);
            float g = std::clamp(yv - 0.1873f * cbv - 0.4681f * crv, 0.0f, 1.0f);
            float b = std::clamp(yv + 1.8556f * cbv,              0.0f, 1.0f);

            int idx = (y * outW + x) * 3;
            out[idx + 0] = r;
            out[idx + 1] = g;
            out[idx + 2] = b;
        }
    }
}

// ── RGB float → YUV420P VideoFrame ───────────────────────────────────────────
video::VideoFramePtr RIFEInterpolator::rgbFloatToFrame(
    const std::vector<float>& rgb, int width, int height,
    const video::VideoFrame& ref)
{
    auto out = std::make_shared<video::VideoFrame>(
        width, height, video::PixelFormat::YUV420P);

    uint8_t* Y  = out->data(0);
    uint8_t* Cb = out->data(1);
    uint8_t* Cr = out->data(2);
    int ls_y  = out->linesize(0);
    int ls_cb = out->linesize(1);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 3;
            float r = rgb[idx + 0];
            float g = rgb[idx + 1];
            float b = rgb[idx + 2];

            // RGB → BT.709 YCbCr
            float yv  =  0.2126f*r + 0.7152f*g + 0.0722f*b;
            float cbv = -0.1146f*r - 0.3854f*g + 0.5000f*b;
            float crv =  0.5000f*r - 0.4542f*g - 0.0458f*b;

            Y[y * ls_y + x] = static_cast<uint8_t>(
                std::clamp(yv * 219.0f + 16.0f, 0.0f, 255.0f));

            if ((y % 2 == 0) && (x % 2 == 0)) {
                Cb[(y/2) * ls_cb + (x/2)] = static_cast<uint8_t>(
                    std::clamp(cbv * 224.0f + 128.0f, 0.0f, 255.0f));
                Cr[(y/2) * ls_cb + (x/2)] = static_cast<uint8_t>(
                    std::clamp(crv * 224.0f + 128.0f, 0.0f, 255.0f));
            }
        }
    }

    out->setTimeBase(ref.timeBase());
    out->setColorMeta(ref.colorMeta());
    return out;
}

// ── interpolate ───────────────────────────────────────────────────────────────
video::VideoFramePtr RIFEInterpolator::interpolate(
    video::VideoFramePtr f0, video::VideoFramePtr f1, float timestep)
{
    if (!m_initialized || !f0 || !f1) return nullptr;

    int padW, padH;
    std::vector<float> rgb0, rgb1;
    yuvToRGBFloat(f0, rgb0, padW, padH);
    yuvToRGBFloat(f1, rgb1, padW, padH);

#ifdef AURORA_NCNN
    if (m_impl->backend == InferenceBackend::NCNN) {
        // Build NCNN mats and run inference
        ncnn::Mat in0 = ncnn::Mat(padW, padH, 3);
        ncnn::Mat in1 = ncnn::Mat(padW, padH, 3);
        memcpy(in0.data, rgb0.data(), rgb0.size() * sizeof(float));
        memcpy(in1.data, rgb1.data(), rgb1.size() * sizeof(float));

        ncnn::Extractor ex = m_impl->net.create_extractor();
        ex.input("img0", in0);
        ex.input("img1", in1);

        // Pass timestep as scalar input
        ncnn::Mat ts(1);
        ((float*)ts.data)[0] = timestep;
        ex.input("timestep", ts);

        ncnn::Mat out;
        ex.extract("output", out);

        // Convert result back
        std::vector<float> outRGB(padW * padH * 3);
        memcpy(outRGB.data(), out.data, outRGB.size() * sizeof(float));
        return rgbFloatToFrame(outRGB, f0->width(), f0->height(), *f0);
    }
#endif

#ifdef AURORA_ONNX
    if (m_impl->backend == InferenceBackend::ONNX) {
        Ort::AllocatorWithDefaultOptions allocator;

        // Build input tensors
        std::vector<int64_t> shape = {1, 3, padH, padW};
        // NCHW layout conversion
        std::vector<float> nchw0(3 * padH * padW);
        std::vector<float> nchw1(3 * padH * padW);
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < padH * padW; ++i) {
                nchw0[c * padH * padW + i] = rgb0[i * 3 + c];
                nchw1[c * padH * padW + i] = rgb1[i * 3 + c];
            }
        }

        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<Ort::Value, 3> inputs = {
            Ort::Value::CreateTensor<float>(memInfo, nchw0.data(), nchw0.size(), shape.data(), 4),
            Ort::Value::CreateTensor<float>(memInfo, nchw1.data(), nchw1.size(), shape.data(), 4),
            Ort::Value::CreateTensor<float>(memInfo, &timestep, 1, std::array<int64_t,1>{1}.data(), 1)
        };

        const char* inputNames[]  = {"img0", "img1", "timestep"};
        const char* outputNames[] = {"output"};
        auto outputs = m_impl->session->Run(Ort::RunOptions{nullptr},
                                            inputNames, inputs.data(), 3,
                                            outputNames, 1);

        float* resultData = outputs[0].GetTensorMutableData<float>();
        // Convert NCHW → HWC
        std::vector<float> hwc(padH * padW * 3);
        for (int c = 0; c < 3; ++c)
            for (int i = 0; i < padH * padW; ++i)
                hwc[i * 3 + c] = resultData[c * padH * padW + i];

        return rgbFloatToFrame(hwc, f0->width(), f0->height(), *f0);
    }
#endif

    return nullptr;
}

} // namespace aurora::interpolation

// ╔══════════════════════════════════════════════════════════════════╗
// ║  Aurora Motion Player — JNI Player Bridge Implementation        ║
// ║  Session 13 Fix: AURORA_STUB_FFMPEG path for CI smoke builds    ║
// ╚══════════════════════════════════════════════════════════════════╝
#include "AuroraPlayerBridge.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AuroraBridge", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AuroraBridge", __VA_ARGS__)

// ════════════════════════════════════════════════════════════════════
// FULL build path (real FFmpeg + NCNN prebuilts available)
// ════════════════════════════════════════════════════════════════════
#ifndef AURORA_STUB_FFMPEG

AuroraPlayerBridge::AuroraPlayerBridge() {
    // Init decoder
    aurora::decoder::DecoderConfig dcfg;
    dcfg.hwAccel = aurora::decoder::HWAccelType::MediaCodec;
    m_decoder = std::make_unique<aurora::decoder::FFmpegDecoder>(dcfg);
    m_decoder->setVideoCallback([this](aurora::video::VideoFramePtr f) {
        onVideoFrame(std::move(f));
    });
    m_decoder->setErrorCallback([](const std::string& e) { LOGE("%s", e.c_str()); });

    // AI Frame Interpolation
    m_flow = std::make_unique<aurora::interpolation::AuroraFlow>();
    m_flow->setOutputCallback([this](aurora::video::VideoFramePtr f) {
        renderFrameToSurface(f);
    });

    // Audio
    aurora::audio::AudioEngineConfig acfg;
    m_audio = std::make_unique<aurora::audio::AudioEngine>(acfg);
    m_audio->init();

    // Subtitle
    m_subtitle = std::make_unique<aurora::subtitle::SubtitleEngine>();

    // Benchmark
    m_bench = std::make_unique<aurora::benchmark::BenchmarkSystem>();

    LOGI("AuroraPlayerBridge created (full mode)");
}

AuroraPlayerBridge::~AuroraPlayerBridge() {
    stop();
    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
}

void AuroraPlayerBridge::setSurface(ANativeWindow* window) {
    if (m_window) ANativeWindow_release(m_window);
    m_window = window;
    if (m_window) ANativeWindow_acquire(m_window);
}

void AuroraPlayerBridge::resize(int w, int h) {
    m_width = w; m_height = h;
}

bool AuroraPlayerBridge::open(const std::string& path) {
    stop();
    if (!m_decoder->open(path)) return false;
    const auto& meta = m_decoder->metadata();
    if (auto* vs = meta.primaryVideo()) {
        aurora::interpolation::InterpolationConfig icfg;
        icfg.sourceFPS = static_cast<float>(vs->frameRate);
        icfg.targetFPS = 60.0f;
        icfg.backend   = aurora::interpolation::InferenceBackend::NCNN;
        icfg.modelPath = "/data/data/com.aurora.player/files/models/rife";
        m_flow->init(icfg);
    }
    m_bench->start();
    m_decoder->play();
    m_playing = true;
    return true;
}

void AuroraPlayerBridge::play() {
    if (!m_playing && m_decoder) { m_decoder->play(); m_playing = true; }
}

void AuroraPlayerBridge::pause() {
    if (m_playing && m_decoder)  { m_decoder->pause(); m_playing = false; }
}

void AuroraPlayerBridge::stop() {
    if (m_decoder) m_decoder->close();
    if (m_flow)    m_flow->flush();
    if (m_bench)   m_bench->stop();
    m_playing = false;
}

void AuroraPlayerBridge::seek(double seconds) {
    if (m_decoder) m_decoder->seek(seconds);
}

double AuroraPlayerBridge::position() const {
    return m_decoder ? m_decoder->position() : 0.0;
}

double AuroraPlayerBridge::duration() const {
    return m_decoder ? m_decoder->duration() : 0.0;
}

void AuroraPlayerBridge::setVolume(int percent) {
    if (m_audio) m_audio->setVolume(percent / 100.0f);
}

void AuroraPlayerBridge::setInterpolation(bool enabled, float targetFPS) {
    m_interpEnabled = enabled;
    (void)targetFPS;
}

void AuroraPlayerBridge::setUpscaler(const std::string& model) {
    aurora::upscaler::UpscaleModel m = aurora::upscaler::UpscaleModel::RealESRGAN;
    if      (model == "Anime4K") m = aurora::upscaler::UpscaleModel::Anime4K;
    else if (model == "SPAN")    m = aurora::upscaler::UpscaleModel::SPAN;
    else if (model == "FSRCNN")  m = aurora::upscaler::UpscaleModel::FSRCNN;
    m_upscaler = aurora::upscaler::UpscalerFactory::create(m);
}

void AuroraPlayerBridge::loadSubtitle(const std::string& path) {
    if (m_subtitle) m_subtitle->loadFile(path);
}

std::string AuroraPlayerBridge::getBenchmarkStats() const {
    if (!m_bench) return "{}";
    auto snap = m_bench->snapshot();
    return aurora::benchmark::BenchmarkSystem::format(snap);
}

void AuroraPlayerBridge::onVideoFrame(aurora::video::VideoFramePtr frame) {
    if (!frame) return;
    m_bench->onFrameDecoded();
    if (m_interpEnabled && m_flow) {
        m_flow->push(frame);
    } else {
        renderFrameToSurface(frame);
    }
}

void AuroraPlayerBridge::renderFrameToSurface(aurora::video::VideoFramePtr frame) {
    if (!m_window || !frame) return;
    m_bench->onRenderBegin();

    ANativeWindow_Buffer buf;
    ARect bounds = {0, 0, m_width, m_height};
    if (ANativeWindow_lock(m_window, &buf, &bounds) != 0) return;

    const uint8_t* y  = frame->data(0);
    const uint8_t* cb = frame->data(1);
    const uint8_t* cr = frame->data(2);
    if (!y || !cb || !cr) { ANativeWindow_unlockAndPost(m_window); return; }

    int ls_y  = frame->linesize(0);
    int ls_cb = frame->linesize(1);
    int fw    = std::min(frame->width(),  m_width);
    int fh    = std::min(frame->height(), m_height);
    auto* dst = static_cast<uint32_t*>(buf.bits);

    for (int row = 0; row < fh; ++row) {
        for (int col = 0; col < fw; ++col) {
            float yv  = (y [row      * ls_y  + col      ] - 16)  / 219.0f;
            float cbv = (cb[(row/2)  * ls_cb + (col/2)  ] - 128) / 224.0f;
            float crv = (cr[(row/2)  * ls_cb + (col/2)  ] - 128) / 224.0f;
            auto clamp255 = [](float v) -> uint8_t {
                return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v * 255.0f)));
            };
            uint8_t r = clamp255(yv + 1.5748f * crv);
            uint8_t g = clamp255(yv - 0.1873f * cbv - 0.4681f * crv);
            uint8_t b = clamp255(yv + 1.8556f * cbv);
            dst[row * buf.stride + col] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }
    ANativeWindow_unlockAndPost(m_window);
    m_bench->onRenderEnd();
    m_bench->onFrameRendered();
}

// ════════════════════════════════════════════════════════════════════
// STUB build path (CI smoke mode — no prebuilt libs)
// All methods are no-ops; APK compiles and links cleanly.
// ════════════════════════════════════════════════════════════════════
#else  // AURORA_STUB_FFMPEG

AuroraPlayerBridge::AuroraPlayerBridge() {
    m_bench = std::make_unique<aurora::benchmark::BenchmarkSystem>();
    LOGI("AuroraPlayerBridge created (STUB / CI mode — no FFmpeg prebuilt)");
}

AuroraPlayerBridge::~AuroraPlayerBridge() {
    if (m_window) { ANativeWindow_release(m_window); m_window = nullptr; }
}

void   AuroraPlayerBridge::setSurface(ANativeWindow* w)  { m_window = w; }
void   AuroraPlayerBridge::resize(int w, int h)          { m_width=w; m_height=h; }
bool   AuroraPlayerBridge::open(const std::string&)      { return false; }
void   AuroraPlayerBridge::play()                        {}
void   AuroraPlayerBridge::pause()                       {}
void   AuroraPlayerBridge::stop()                        {}
void   AuroraPlayerBridge::seek(double)                  {}
double AuroraPlayerBridge::position()              const { return 0.0; }
double AuroraPlayerBridge::duration()              const { return 0.0; }
void   AuroraPlayerBridge::setVolume(int)                {}
void   AuroraPlayerBridge::setInterpolation(bool, float) {}
void   AuroraPlayerBridge::setUpscaler(const std::string&) {}
void   AuroraPlayerBridge::loadSubtitle(const std::string&) {}
std::string AuroraPlayerBridge::getBenchmarkStats() const { return "{}"; }
void   AuroraPlayerBridge::onVideoFrame(aurora::video::VideoFramePtr) {}
void   AuroraPlayerBridge::renderFrameToSurface(aurora::video::VideoFramePtr) {}

#endif // AURORA_STUB_FFMPEG

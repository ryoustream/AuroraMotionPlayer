// ╔══════════════════════════════════════════════════════════════════╗
// ║  Aurora Motion Player — Android JNI Player Bridge               ║
// ║  Session 13 Fix: AURORA_STUB guards for CI builds without       ║
// ║  FFmpeg/NCNN prebuilts; correct include paths via CMake root    ║
// ╚══════════════════════════════════════════════════════════════════╝
#pragma once
#include <android/native_window.h>
#include <memory>
#include <string>
#include <atomic>

// ── Conditional core includes ─────────────────────────────────────────────────
// When building in CI smoke mode (no prebuilt FFmpeg/NCNN), AURORA_STUB_FFMPEG
// is defined by CMake. In that case we use lightweight stub types so the JNI
// layer compiles and the APK links without native libraries.
#ifndef AURORA_STUB_FFMPEG
#  include "decoder/FFmpegDecoder.h"
#  include "interpolation/AuroraFlow.h"
#  include "upscaler/UpscalerFactory.h"
#  include "audio/AudioEngine.h"
#  include "subtitle/SubtitleEngine.h"
#  include "benchmark/BenchmarkSystem.h"
#endif

#ifdef AURORA_STUB_FFMPEG
// ── Stub types for CI builds without prebuilt libraries ───────────────────────
namespace aurora {
namespace decoder  { struct FFmpegDecoder { bool open(const std::string&){return false;} void play(){} void pause(){} void close(){} void seek(double){} double position()const{return 0;} double duration()const{return 0;} }; }
namespace interpolation { struct AuroraFlow { void push(void*){}; void flush(){} void setOutputCallback(auto){}; void init(auto&){}; float inputFPS()const{return 0;} }; }
namespace upscaler  { struct UpscalerBase{}; struct UpscalerFactory{ static std::unique_ptr<UpscalerBase> create(int){return {};} }; }
namespace audio     { struct AudioEngineConfig{}; struct AudioEngine { AudioEngine(AudioEngineConfig&){} void init(){} void setVolume(float){} }; }
namespace subtitle  { struct SubtitleEngine { void loadFile(const std::string&){} }; }
namespace benchmark { struct BenchmarkSystem { void start(){} void stop(){} void onFrameDecoded(){} void onRenderBegin(){} void onRenderEnd(){} void onFrameRendered(){} struct Snapshot{}; Snapshot snapshot()const{return {};} static std::string format(const Snapshot&){return "{}";} }; }
namespace video     { struct VideoFrame{}; using VideoFramePtr = std::shared_ptr<VideoFrame>; }
}
#endif

class AuroraPlayerBridge {
public:
    AuroraPlayerBridge();
    ~AuroraPlayerBridge();

    void   setSurface(ANativeWindow* window);
    void   resize(int w, int h);
    bool   open(const std::string& path);
    void   play();
    void   pause();
    void   stop();
    void   seek(double seconds);
    double position() const;
    double duration() const;
    void   setVolume(int percent);
    void   setInterpolation(bool enabled, float targetFPS);
    void   setUpscaler(const std::string& model);
    void   loadSubtitle(const std::string& path);
    std::string getBenchmarkStats() const;

private:
    void onVideoFrame(aurora::video::VideoFramePtr frame);
    void renderFrameToSurface(aurora::video::VideoFramePtr frame);

    ANativeWindow* m_window  = nullptr;
    int  m_width  = 0, m_height = 0;
    bool m_playing       = false;
    bool m_interpEnabled = true;

    std::unique_ptr<aurora::decoder::FFmpegDecoder>     m_decoder;
    std::unique_ptr<aurora::interpolation::AuroraFlow>  m_flow;
    std::unique_ptr<aurora::upscaler::UpscalerBase>     m_upscaler;
    std::unique_ptr<aurora::audio::AudioEngine>         m_audio;
    std::unique_ptr<aurora::subtitle::SubtitleEngine>   m_subtitle;
    std::unique_ptr<aurora::benchmark::BenchmarkSystem> m_bench;
};

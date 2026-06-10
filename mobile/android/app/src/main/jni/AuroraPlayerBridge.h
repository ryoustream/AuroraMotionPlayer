#pragma once
#include <android/native_window.h>
#include <memory>
#include <string>
#include <atomic>

#include "decoder/FFmpegDecoder.h"
#include "interpolation/AuroraFlow.h"
#include "upscaler/UpscalerFactory.h"
#include "audio/AudioEngine.h"
#include "subtitle/SubtitleEngine.h"
#include "benchmark/BenchmarkSystem.h"

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

    ANativeWindow* m_window = nullptr;
    int  m_width  = 0, m_height = 0;
    bool m_playing = false;
    bool m_interpEnabled = true;

    std::unique_ptr<aurora::decoder::FFmpegDecoder>     m_decoder;
    std::unique_ptr<aurora::interpolation::AuroraFlow>  m_flow;
    std::unique_ptr<aurora::upscaler::UpscalerBase>     m_upscaler;
    std::unique_ptr<aurora::audio::AudioEngine>         m_audio;
    std::unique_ptr<aurora::subtitle::SubtitleEngine>   m_subtitle;
    std::unique_ptr<aurora::benchmark::BenchmarkSystem> m_bench;
};

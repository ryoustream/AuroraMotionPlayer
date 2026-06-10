#pragma once
#include <QWidget>
#include <memory>
#include <string>

namespace aurora::decoder  { class FFmpegDecoder; }
namespace aurora::renderer { class RendererBase;  }
namespace aurora::interpolation { class AuroraFlow; }
namespace aurora::upscaler { class UpscalerBase; }
namespace aurora::hdr      { class HDREngine;    }
namespace aurora::audio    { class AudioEngine;  }
namespace aurora::subtitle { class SubtitleEngine; }
namespace aurora::benchmark{ class BenchmarkSystem; }
namespace aurora::scene    { class SceneDetector; }

class PlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayerWidget(QWidget* parent = nullptr);
    ~PlayerWidget() override;

    bool   open(const QString& path);
    void   stop();
    void   togglePlayPause();
    void   seek(double seconds);
    void   seekRelative(double delta);
    void   setVolume(int percent);
    int    volume() const noexcept { return m_volume; }
    double position() const noexcept;
    double duration() const noexcept;
    bool   isPlaying() const noexcept { return m_playing; }

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void errorOccurred(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    QPaintEngine* paintEngine() const override { return nullptr; } // native rendering

private:
    void initPipeline();
    void onVideoFrame(aurora::video::VideoFramePtr frame);
    void onAudioBuffer(aurora::audio::AudioBufferPtr buf);
    void onError(const std::string& msg);

    std::unique_ptr<aurora::decoder::FFmpegDecoder>         m_decoder;
    std::unique_ptr<aurora::renderer::RendererBase>         m_renderer;
    std::unique_ptr<aurora::interpolation::AuroraFlow>      m_flow;
    std::unique_ptr<aurora::upscaler::UpscalerBase>         m_upscaler;
    std::unique_ptr<aurora::hdr::HDREngine>                 m_hdr;
    std::unique_ptr<aurora::audio::AudioEngine>             m_audio;
    std::unique_ptr<aurora::subtitle::SubtitleEngine>       m_subtitle;
    std::unique_ptr<aurora::benchmark::BenchmarkSystem>     m_bench;
    std::unique_ptr<aurora::scene::SceneDetector>           m_scene;

    bool   m_playing = false;
    int    m_volume  = 100;
};

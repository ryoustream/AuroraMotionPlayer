#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "video/VideoFrame.h"
#include "video/VideoMetadata.h"
#include "audio/AudioBuffer.h"

// Forward declare FFmpeg structs to avoid polluting headers
struct AVFormatContext;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;

namespace aurora::decoder {

enum class DecoderState {
    Idle,
    Opening,
    Playing,
    Paused,
    Seeking,
    EOF_,
    Error,
};

enum class HWAccelType {
    None,
    NVDEC,
    D3D11VA,
    DXVA2,
    QSV,
    MediaCodec,  // Android
    Vulkan,
};

struct DecoderConfig {
    HWAccelType hwAccel        = HWAccelType::None;
    bool        enableHDR      = true;
    int         threadCount    = 0;   // 0 = auto
    int         videoQueueSize = 16;
    int         audioQueueSize = 64;
};

class FFmpegDecoder {
public:
    using VideoCallback = std::function<void(video::VideoFramePtr)>;
    using AudioCallback = std::function<void(audio::AudioBufferPtr)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    explicit FFmpegDecoder(DecoderConfig cfg = {});
    ~FFmpegDecoder();

    // Non-copyable
    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    bool open(const std::string& url);
    void close();

    void play();
    void pause();
    void seek(double seconds);
    void setSpeed(double speed);

    DecoderState          state()    const noexcept { return m_state; }
    const video::VideoMetadata& metadata() const noexcept { return m_metadata; }
    double                position() const noexcept;
    double                duration() const noexcept;

    void setVideoCallback(VideoCallback cb) { m_videoCb = std::move(cb); }
    void setAudioCallback(AudioCallback cb) { m_audioCb = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { m_errorCb = std::move(cb); }

    void setPreferHardware(bool prefer) noexcept { m_config.hwAccel = prefer ? HWAccelType::D3D11VA : HWAccelType::None; }

    // Stream selection
    void selectVideoStream(int index);
    void selectAudioStream(int index);

private:
    void demuxLoop();
    void videoDecodeLoop();
    void audioDecodeLoop();
    bool initHWAccel();
    bool openVideoCodec(int streamIdx);
    bool openAudioCodec(int streamIdx);
    void handleError(const std::string& msg);

    DecoderConfig           m_config;
    std::atomic<DecoderState> m_state{DecoderState::Idle};

    // FFmpeg contexts (opaque pointers, allocated/freed in .cpp)
    AVFormatContext* m_fmtCtx     = nullptr;
    AVCodecContext*  m_vCodecCtx  = nullptr;
    AVCodecContext*  m_aCodecCtx  = nullptr;

    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;

    video::VideoMetadata m_metadata;

    // Decode threads
    std::thread m_demuxThread;
    std::thread m_videoThread;
    std::thread m_audioThread;
    std::atomic<bool> m_stopThreads{false};

    // Callbacks
    VideoCallback m_videoCb;
    AudioCallback m_audioCb;
    ErrorCallback m_errorCb;

    // Timing
    std::atomic<double> m_position{0.0};
    std::atomic<double> m_speed{1.0};
};

} // namespace aurora::decoder

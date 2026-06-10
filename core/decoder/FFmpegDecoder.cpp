#include "FFmpegDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <stdexcept>
#include <cassert>
#include <chrono>
#include <cstring>

namespace aurora::decoder {

// ── RAII helper for AVFrame ──────────────────────────────────────────────────
struct AVFrameGuard {
    AVFrame* frame = av_frame_alloc();
    ~AVFrameGuard() { av_frame_free(&frame); }
    AVFrame* get()  { return frame; }
    AVFrame* operator->() { return frame; }
};

// ── RAII helper for AVPacket ─────────────────────────────────────────────────
struct AVPacketGuard {
    AVPacket* pkt = av_packet_alloc();
    ~AVPacketGuard() { av_packet_free(&pkt); }
    AVPacket* get() { return pkt; }
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
FFmpegDecoder::FFmpegDecoder(DecoderConfig cfg)
    : m_config(std::move(cfg))
{
    // avformat_network_init() is safe to call multiple times
    avformat_network_init();
}

FFmpegDecoder::~FFmpegDecoder() {
    close();
}

// ── open ─────────────────────────────────────────────────────────────────────
bool FFmpegDecoder::open(const std::string& url) {
    m_state = DecoderState::Opening;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "timeout",        "5000000", 0); // 5s

    int ret = avformat_open_input(&m_fmtCtx, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        handleError(std::string("avformat_open_input: ") + err);
        return false;
    }

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        handleError("avformat_find_stream_info failed");
        return false;
    }

    // ── Populate metadata ────────────────────────────────────────────────────
    m_metadata.filePath    = url;
    m_metadata.formatName  = m_fmtCtx->iformat->long_name
                             ? m_fmtCtx->iformat->long_name
                             : m_fmtCtx->iformat->name;
    m_metadata.duration    = m_fmtCtx->duration > 0
                             ? static_cast<double>(m_fmtCtx->duration) / AV_TIME_BASE
                             : 0.0;
    m_metadata.totalBitrate = m_fmtCtx->bit_rate;
    m_metadata.fileSize     = m_fmtCtx->pb
                             ? avio_size(m_fmtCtx->pb)
                             : 0;

    // Parse tags
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(m_fmtCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        m_metadata.tags[tag->key] = tag->value;
    }

    // ── Parse streams ────────────────────────────────────────────────────────
    for (unsigned i = 0; i < m_fmtCtx->nb_streams; ++i) {
        AVStream* st = m_fmtCtx->streams[i];
        AVCodecParameters* par = st->codecpar;
        double tb = av_q2d(st->time_base);

        if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
            video::VideoStream vs;
            vs.index     = static_cast<int>(i);
            vs.codecName = avcodec_get_name(par->codec_id);
            vs.width     = par->width;
            vs.height    = par->height;
            vs.frameRate = av_q2d(st->r_frame_rate);
            vs.bitrate   = par->bit_rate;
            vs.duration  = (st->duration > 0)
                           ? st->duration * tb
                           : m_metadata.duration;

            // HDR detection
            if (par->color_trc == AVCOL_TRC_SMPTE2084 ||
                par->color_trc == AVCOL_TRC_ARIB_STD_B67) {
                vs.isHDR = true;
            }

            if (m_videoStreamIdx < 0) m_videoStreamIdx = static_cast<int>(i);
            m_metadata.videoStreams.push_back(std::move(vs));

        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
            video::AudioStream as;
            as.index      = static_cast<int>(i);
            as.codecName  = avcodec_get_name(par->codec_id);
            as.sampleRate = par->sample_rate;
            as.bitrate    = par->bit_rate;
            as.channels   = par->ch_layout.nb_channels;

            auto* lang = av_dict_get(st->metadata, "language", nullptr, 0);
            if (lang) as.language = lang->value;
            auto* title = av_dict_get(st->metadata, "title", nullptr, 0);
            if (title) as.title = title->value;

            if (m_audioStreamIdx < 0) m_audioStreamIdx = static_cast<int>(i);
            m_metadata.audioStreams.push_back(std::move(as));

        } else if (par->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            video::SubtitleStream ss;
            ss.index     = static_cast<int>(i);
            ss.codecName = avcodec_get_name(par->codec_id);

            auto* lang = av_dict_get(st->metadata, "language", nullptr, 0);
            if (lang) ss.language = lang->value;

            m_metadata.subtitleStreams.push_back(std::move(ss));
        }
    }

    // ── Parse chapters ───────────────────────────────────────────────────────
    for (int i = 0; i < (int)m_fmtCtx->nb_chapters; ++i) {
        AVChapter* ch = m_fmtCtx->chapters[i];
        video::ChapterInfo ci;
        ci.startPts  = ch->start;
        ci.endPts    = ch->end;
        ci.startTime = ch->start * av_q2d(ch->time_base);
        ci.endTime   = ch->end   * av_q2d(ch->time_base);

        auto* title = av_dict_get(ch->metadata, "title", nullptr, 0);
        if (title) ci.title = title->value;
        m_metadata.chapters.push_back(std::move(ci));
    }

    // Open codecs
    if (m_videoStreamIdx >= 0 && !openVideoCodec(m_videoStreamIdx)) return false;
    if (m_audioStreamIdx >= 0 && !openAudioCodec(m_audioStreamIdx)) return false;

    m_state = DecoderState::Paused;
    return true;
}

// ── openVideoCodec ────────────────────────────────────────────────────────────
bool FFmpegDecoder::openVideoCodec(int streamIdx) {
    AVStream* st = m_fmtCtx->streams[streamIdx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        handleError("No video decoder found for codec: " +
                    std::string(avcodec_get_name(st->codecpar->codec_id)));
        return false;
    }

    m_vCodecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_vCodecCtx, st->codecpar);

    if (m_config.threadCount > 0) {
        m_vCodecCtx->thread_count = m_config.threadCount;
    }
    m_vCodecCtx->thread_type = FF_THREAD_FRAME;

    // HW accel setup (platform-specific)
    initHWAccel();

    if (avcodec_open2(m_vCodecCtx, codec, nullptr) < 0) {
        handleError("Failed to open video codec");
        return false;
    }
    return true;
}

// ── openAudioCodec ────────────────────────────────────────────────────────────
bool FFmpegDecoder::openAudioCodec(int streamIdx) {
    AVStream* st = m_fmtCtx->streams[streamIdx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        handleError("No audio decoder found");
        return false;
    }
    m_aCodecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_aCodecCtx, st->codecpar);
    if (avcodec_open2(m_aCodecCtx, codec, nullptr) < 0) {
        handleError("Failed to open audio codec");
        return false;
    }
    return true;
}

// ── initHWAccel ───────────────────────────────────────────────────────────────
bool FFmpegDecoder::initHWAccel() {
    if (m_config.hwAccel == HWAccelType::None) return false;
    // Platform-specific HW accel initialization
    // Actual implementation hooks into FFmpeg's hw_device_ctx API
    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
    switch (m_config.hwAccel) {
#ifdef _WIN32
    case HWAccelType::D3D11VA:  hwType = AV_HWDEVICE_TYPE_D3D11VA;  break;
    case HWAccelType::DXVA2:    hwType = AV_HWDEVICE_TYPE_DXVA2;    break;
    case HWAccelType::NVDEC:    hwType = AV_HWDEVICE_TYPE_CUDA;     break;
    case HWAccelType::QSV:      hwType = AV_HWDEVICE_TYPE_QSV;      break;
#endif
#ifdef __ANDROID__
    case HWAccelType::MediaCodec: hwType = AV_HWDEVICE_TYPE_MEDIACODEC; break;
#endif
    case HWAccelType::Vulkan:   hwType = AV_HWDEVICE_TYPE_VULKAN;   break;
    default: return false;
    }

    AVBufferRef* hwDevCtx = nullptr;
    if (av_hwdevice_ctx_create(&hwDevCtx, hwType, nullptr, nullptr, 0) < 0) {
        return false; // Fall back to SW decode
    }
    m_vCodecCtx->hw_device_ctx = av_buffer_ref(hwDevCtx);
    av_buffer_unref(&hwDevCtx);
    return true;
}

// ── play / pause / seek ───────────────────────────────────────────────────────
void FFmpegDecoder::play() {
    if (m_state == DecoderState::Paused || m_state == DecoderState::Idle) {
        m_state = DecoderState::Playing;
        m_stopThreads = false;
        m_demuxThread  = std::thread(&FFmpegDecoder::demuxLoop,  this);
        m_videoThread  = std::thread(&FFmpegDecoder::videoDecodeLoop, this);
        m_audioThread  = std::thread(&FFmpegDecoder::audioDecodeLoop, this);
    }
}

void FFmpegDecoder::pause() {
    m_state = DecoderState::Paused;
}

void FFmpegDecoder::seek(double seconds) {
    if (!m_fmtCtx) return;
    m_state = DecoderState::Seeking;
    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
    av_seek_frame(m_fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD);
    if (m_vCodecCtx) avcodec_flush_buffers(m_vCodecCtx);
    if (m_aCodecCtx) avcodec_flush_buffers(m_aCodecCtx);
    m_state = DecoderState::Playing;
}

void FFmpegDecoder::setSpeed(double speed) {
    m_speed = speed;
}

double FFmpegDecoder::position() const noexcept {
    return m_position.load();
}

double FFmpegDecoder::duration() const noexcept {
    return m_metadata.duration;
}

// ── close ─────────────────────────────────────────────────────────────────────
void FFmpegDecoder::close() {
    m_stopThreads = true;
    m_state = DecoderState::Idle;

    if (m_demuxThread.joinable())  m_demuxThread.join();
    if (m_videoThread.joinable())  m_videoThread.join();
    if (m_audioThread.joinable())  m_audioThread.join();

    if (m_vCodecCtx)  { avcodec_free_context(&m_vCodecCtx); }
    if (m_aCodecCtx)  { avcodec_free_context(&m_aCodecCtx); }
    if (m_fmtCtx)     { avformat_close_input(&m_fmtCtx); }

    m_videoStreamIdx = -1;
    m_audioStreamIdx = -1;
    m_position = 0.0;
}

// ── demuxLoop ─────────────────────────────────────────────────────────────────
void FFmpegDecoder::demuxLoop() {
    AVPacketGuard pkt;
    while (!m_stopThreads) {
        if (m_state == DecoderState::Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        int ret = av_read_frame(m_fmtCtx, pkt.get());
        if (ret < 0) {
            m_state = DecoderState::EOF_;
            break;
        }
        // Route to appropriate codec context
        if (pkt.get()->stream_index == m_videoStreamIdx && m_vCodecCtx) {
            avcodec_send_packet(m_vCodecCtx, pkt.get());
        } else if (pkt.get()->stream_index == m_audioStreamIdx && m_aCodecCtx) {
            avcodec_send_packet(m_aCodecCtx, pkt.get());
        }
        av_packet_unref(pkt.get());
    }
}

// ── videoDecodeLoop ───────────────────────────────────────────────────────────
void FFmpegDecoder::videoDecodeLoop() {
    AVFrameGuard frame;
    while (!m_stopThreads) {
        if (!m_vCodecCtx) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        int ret = avcodec_receive_frame(m_vCodecCtx, frame.get());
        if (ret == AVERROR(EAGAIN)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (ret < 0) break;

        // Convert to aurora VideoFrame and dispatch
        auto vf = std::make_shared<video::VideoFrame>(
            frame->width, frame->height, video::PixelFormat::YUV420P);

        double tb = av_q2d(m_fmtCtx->streams[m_videoStreamIdx]->time_base);
        vf->setPts(frame->pts);
        vf->setTimeBase(tb);
        m_position = frame->pts * tb;

        if (m_videoCb) m_videoCb(std::move(vf));

        av_frame_unref(frame.get());
    }
}

// ── audioDecodeLoop ───────────────────────────────────────────────────────────
void FFmpegDecoder::audioDecodeLoop() {
    AVFrameGuard frame;
    while (!m_stopThreads) {
        if (!m_aCodecCtx) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        int ret = avcodec_receive_frame(m_aCodecCtx, frame.get());
        if (ret == AVERROR(EAGAIN)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (ret < 0) break;

        // TODO: Convert to AudioBuffer and dispatch
        av_frame_unref(frame.get());
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void FFmpegDecoder::handleError(const std::string& msg) {
    m_state = DecoderState::Error;
    if (m_errorCb) m_errorCb(msg);
}

void FFmpegDecoder::selectVideoStream(int index) {
    m_videoStreamIdx = index;
}

void FFmpegDecoder::selectAudioStream(int index) {
    m_audioStreamIdx = index;
}

} // namespace aurora::decoder

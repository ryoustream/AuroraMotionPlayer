#pragma once
/**
 * Aurora Motion Player — AAudio Output (Android)
 *
 * Low-latency audio output using Android AAudio API (API 26+).
 * Falls back to OpenSL ES on older devices.
 *
 * Features:
 *  - Exclusive / Shared performance mode
 *  - Low-latency exclusive stream for Adreno 640+ devices
 *  - Float32 and Int16 PCM
 *  - Automatic reconnection on device change
 *  - Background playback support
 */

#ifdef __ANDROID__

#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace aurora::core {

// ── Audio chunk (reuse from WASAPI header on cross-platform) ──────────────────
struct AudioChunkAndroid {
    std::vector<float> samples;   // interleaved float PCM
    int64_t            pts = 0;   // μs
};

// ── AAudio Output ─────────────────────────────────────────────────────────────
class AAudioOutput {
public:
    AAudioOutput();
    ~AAudioOutput();

    // Non-copyable
    AAudioOutput(const AAudioOutput&)            = delete;
    AAudioOutput& operator=(const AAudioOutput&) = delete;

    // ----- Configuration -----------------------------------------------------
    struct Config {
        int   sampleRate    = 48000;
        int   channels      = 2;
        bool  lowLatency    = true;   // AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
        float volume        = 1.0f;
        int   bufferCapacity = 0;    // 0 = auto
    };

    // ----- Lifecycle ---------------------------------------------------------
    bool open(const Config& cfg);
    void close();
    bool isOpen() const { return m_open.load(); }

    bool  start();
    void  stop();
    void  pause();
    void  resume();
    void  flush();

    // ----- Frame input -------------------------------------------------------
    bool  pushChunk(AudioChunkAndroid chunk);

    // ----- Info --------------------------------------------------------------
    int     sampleRate()  const { return m_sampleRate; }
    int     channels()    const { return m_channels; }
    int64_t positionUs()  const;
    int     latencyMs()   const;
    int     bufferFrames() const { return m_bufferFrames; }

    // ----- Volume ------------------------------------------------------------
    void  setVolume(float vol) { m_volume.store(vol); }
    float volume() const       { return m_volume.load(); }

    // ----- Underrun callback -------------------------------------------------
    using UnderrunCb = std::function<void()>;
    void onUnderrun(UnderrunCb cb) { m_underrunCb = std::move(cb); }

private:
    static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void*         userData,
        void*         audioData,
        int32_t       numFrames);

    static void errorCallback(
        AAudioStream* stream,
        void*         userData,
        aaudio_result_t error);

    void handleError(aaudio_result_t error);
    bool restartStream();

    AAudioStream*            m_stream       = nullptr;
    std::atomic<bool>        m_open         {false};
    std::atomic<bool>        m_playing      {false};
    std::atomic<float>       m_volume       {1.0f};

    int                      m_sampleRate   = 48000;
    int                      m_channels     = 2;
    int                      m_bufferFrames = 0;

    // Audio queue (lock-free ring buffer would be ideal; using mutex for clarity)
    mutable std::mutex       m_mutex;
    std::queue<AudioChunkAndroid> m_queue;
    std::vector<float>       m_remainder;   // leftover from last chunk
    static constexpr int     k_MaxChunks = 32;

    mutable std::mutex       m_posMutex;
    int64_t                  m_positionUs = 0;

    UnderrunCb               m_underrunCb;
    Config                   m_config;
};

} // namespace aurora::core

#endif // __ANDROID__

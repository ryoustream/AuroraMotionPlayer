#pragma once
/**
 * Aurora Motion Player — WASAPI Audio Output
 *
 * Windows Audio Session API output backend.
 * Supports:
 *  - Exclusive mode (lowest latency, bitperfect)
 *  - Shared mode (compatible with other apps)
 *  - HDMI/SPDIF passthrough (AC3, EAC3, TrueHD, DTS, DTS-HD)
 *  - Float32 and Int16 PCM
 *  - Sample rate / channel conversion fallback
 */

#ifdef _WIN32

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace aurora::core {

// ── Audio format ──────────────────────────────────────────────────────────────
struct AudioFormat {
    int   sampleRate  = 48000;
    int   channels    = 2;
    int   bitsPerSample = 32;   // 16 or 32
    bool  isFloat     = true;
    bool  isPassthrough = false;

    bool operator==(const AudioFormat& o) const {
        return sampleRate   == o.sampleRate   &&
               channels     == o.channels     &&
               bitsPerSample == o.bitsPerSample &&
               isFloat       == o.isFloat      &&
               isPassthrough == o.isPassthrough;
    }
};

// ── Audio chunk ───────────────────────────────────────────────────────────────
struct AudioChunk {
    std::vector<uint8_t> data;
    AudioFormat          format;
    int64_t              pts = 0;   // presentation timestamp (μs)
};

// ── WASAPI mode ───────────────────────────────────────────────────────────────
enum class WASAPIMode { Shared, Exclusive };

// ── WASAPI Output ─────────────────────────────────────────────────────────────
class WASAPIOutput {
public:
    WASAPIOutput();
    ~WASAPIOutput();

    // Non-copyable
    WASAPIOutput(const WASAPIOutput&)            = delete;
    WASAPIOutput& operator=(const WASAPIOutput&) = delete;

    // ----- Configuration -----------------------------------------------------
    struct Config {
        WASAPIMode  mode          = WASAPIMode::Shared;
        AudioFormat format        = {};
        bool        autoFormat    = true;   // use device native format
        int         bufferMs      = 50;     // requested buffer size
        float       volume        = 1.0f;
        std::string deviceId      = "";     // "" = default device
    };

    // ----- Lifecycle ---------------------------------------------------------
    bool open(const Config& cfg);
    void close();
    bool isOpen() const { return m_open.load(); }

    // ----- Playback ----------------------------------------------------------
    bool  start();
    void  stop();
    void  pause();
    void  resume();
    void  flush();

    /// Push audio data. Returns false if the queue is full.
    bool  pushChunk(AudioChunk chunk);

    /// Current playback position in microseconds
    int64_t positionUs() const;

    /// Approximate latency in milliseconds
    int     latencyMs() const;

    // ----- Volume ------------------------------------------------------------
    void  setVolume(float vol);  // 0.0 – 1.0
    float volume() const { return m_volume.load(); }

    // ----- Device enumeration ------------------------------------------------
    struct DeviceInfo {
        std::string id;
        std::wstring name;
        bool        isDefault = false;
    };
    static std::vector<DeviceInfo> enumerateDevices();

    // ----- Passthrough -------------------------------------------------------
    bool supportsPassthrough() const;

    // ----- Callbacks ---------------------------------------------------------
    using UnderrunCb = std::function<void()>;
    void onUnderrun(UnderrunCb cb) { m_underrunCb = std::move(cb); }

private:
    bool  initCOM();
    bool  openDevice(const std::string& deviceId);
    bool  negotiateFormat(const AudioFormat& requested);
    bool  createAudioClient(WASAPIMode mode);
    void  renderLoop();
    UINT32 availableFrames();
    bool  writeFrames(const uint8_t* data, UINT32 frames);

    // COM / WASAPI objects
    ComPtr<IMMDeviceEnumerator> m_enumerator;
    ComPtr<IMMDevice>           m_device;
    ComPtr<IAudioClient>        m_audioClient;
    ComPtr<IAudioRenderClient>  m_renderClient;
    ComPtr<ISimpleAudioVolume>  m_volumeControl;

    HANDLE                      m_bufferEvent  = nullptr;
    UINT32                      m_bufferFrames = 0;
    WAVEFORMATEXTENSIBLE        m_wfx          = {};

    // State
    std::atomic<bool>           m_open       {false};
    std::atomic<bool>           m_playing    {false};
    std::atomic<bool>           m_paused     {false};
    std::atomic<float>          m_volume     {1.0f};

    // Audio queue
    mutable std::mutex          m_queueMutex;
    std::condition_variable     m_queueCV;
    std::queue<AudioChunk>      m_queue;
    static constexpr int        k_MaxQueueChunks = 16;

    // Render thread
    std::thread                 m_renderThread;

    // Callback
    UnderrunCb                  m_underrunCb;

    // Position tracking
    mutable std::mutex          m_posMutex;
    int64_t                     m_positionUs = 0;

    Config                      m_config;
};

} // namespace aurora::core

#endif // _WIN32

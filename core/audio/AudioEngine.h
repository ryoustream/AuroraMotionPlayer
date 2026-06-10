#pragma once
#include "AudioBuffer.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aurora::audio {

enum class AudioOutputMode {
    Normal,       // Decoded PCM output
    Passthrough,  // Bitstream passthrough (HDMI/SPDIF)
};

enum class AudioDevice {
    Default,
    WASAPI,   // Windows exclusive
    SPDIF,
    HDMI,
};

struct AudioEngineConfig {
    AudioOutputMode mode        = AudioOutputMode::Normal;
    AudioDevice     device      = AudioDevice::Default;
    int             bufferMs    = 50;    // Output buffer size in ms
    int             sampleRate  = 48000;
    int             channels    = 2;
    float           volume      = 1.0f;
    float           balance     = 0.0f; // -1.0 left, 0 center, 1.0 right
};

class AudioEngine {
public:
    explicit AudioEngine(AudioEngineConfig cfg = {});
    ~AudioEngine();

    bool    init();
    void    shutdown();
    void    push(AudioBufferPtr buf);
    void    flush();
    void    pause();
    void    resume();

    void    setVolume(float vol)         noexcept;
    void    setBalance(float bal)        noexcept;
    void    setOutputMode(AudioOutputMode mode);

    float   volume()  const noexcept { return m_config.volume; }
    float   balance() const noexcept { return m_config.balance; }
    bool    isRunning() const noexcept { return m_running; }
    double  latencyMs() const noexcept;

    // Enumerate available output devices
    static std::vector<std::string> enumerateDevices();

private:
    void outputLoop();

    AudioEngineConfig       m_config;
    std::atomic<bool>       m_running{false};
    std::atomic<bool>       m_paused{false};

    // Platform audio handle (opaque)
    void*                   m_deviceHandle = nullptr;
};

} // namespace aurora::audio

#pragma once
#include "AudioBuffer.h"
#include <functional>

namespace aurora::audio {

// Detects whether a codec supports bitstream passthrough
// and prepares the raw bitstream for HDMI/SPDIF output
class AudioPassthrough {
public:
    using PassthroughCallback = std::function<void(const uint8_t*, size_t)>;

    static bool isPassthroughSupported(AudioCodecType codec) noexcept;
    static bool isPassthroughCapableDevice(const std::string& deviceName) noexcept;

    bool init(AudioCodecType codec, int sampleRate, int channels);
    void process(AudioBufferPtr buf, PassthroughCallback cb);
    void flush();

private:
    AudioCodecType  m_codec      = AudioCodecType::PCM;
    int             m_sampleRate = 48000;
    int             m_channels   = 2;
    bool            m_initialized = false;
};

} // namespace aurora::audio

#include "AudioPassthrough.h"

namespace aurora::audio {

bool AudioPassthrough::isPassthroughSupported(AudioCodecType codec) noexcept {
    switch (codec) {
    case AudioCodecType::AC3:
    case AudioCodecType::EAC3:
    case AudioCodecType::TRUEHD:
    case AudioCodecType::DTS:
    case AudioCodecType::DTSHD:
        return true;
    default:
        return false;
    }
}

bool AudioPassthrough::isPassthroughCapableDevice(const std::string& deviceName) noexcept {
    return deviceName.find("HDMI") != std::string::npos ||
           deviceName.find("SPDIF") != std::string::npos ||
           deviceName.find("Digital") != std::string::npos;
}

bool AudioPassthrough::init(AudioCodecType codec, int sampleRate, int channels) {
    if (!isPassthroughSupported(codec)) return false;
    m_codec       = codec;
    m_sampleRate  = sampleRate;
    m_channels    = channels;
    m_initialized = true;
    return true;
}

void AudioPassthrough::process(AudioBufferPtr buf, PassthroughCallback cb) {
    if (!m_initialized || !buf || !cb) return;
    if (buf->planes.empty() || buf->planes[0].empty()) return;
    // Wrap in IEC 61937 framing for SPDIF/HDMI
    // Full implementation: wraps AC3/DTS/TrueHD in IEC 61937 burst
    cb(buf->planes[0].data(), buf->planes[0].size());
}

void AudioPassthrough::flush() {
    // Drain any buffered IEC frames
}

} // namespace aurora::audio

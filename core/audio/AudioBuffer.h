#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace aurora::audio {

enum class SampleFormat {
    U8,
    S16,
    S32,
    FLT,
    DBL,
    S16P,   // Planar
    FLTP,   // Planar float
};

enum class AudioCodecType {
    PCM,
    AAC,
    MP3,
    AC3,
    EAC3,
    TRUEHD,
    DTS,
    DTSHD,
    OPUS,
    VORBIS,
    FLAC,
};

struct AudioBuffer {
    int64_t     pts         = 0;
    double      timeBase    = 0.0;
    int         sampleRate  = 48000;
    int         channels    = 2;
    int         nbSamples   = 0;
    SampleFormat format     = SampleFormat::FLTP;
    AudioCodecType codec    = AudioCodecType::PCM;

    // Planar data per channel (or single interleaved plane)
    std::vector<std::vector<uint8_t>> planes;

    bool isPassthrough = false; // Raw bitstream for HDMI/SPDIF passthrough

    double timestampSeconds() const noexcept {
        return static_cast<double>(pts) * timeBase;
    }

    size_t bytesPerSample() const noexcept;
    size_t totalBytes()     const noexcept;
};

using AudioBufferPtr = std::shared_ptr<AudioBuffer>;

} // namespace aurora::audio

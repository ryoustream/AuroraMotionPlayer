#pragma once
/**
 * Aurora Motion Player — Audio Resampler
 *
 * Wraps FFmpeg libswresample for:
 *  - Sample rate conversion (e.g. 44100 → 48000)
 *  - Channel layout remapping (e.g. 5.1 → stereo)
 *  - Sample format conversion (S16 / S32 / FLTP)
 *  - Tempo-preserving time-stretching via simple linear interpolation
 */

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <cstdint>
#include <vector>
#include <memory>

namespace aurora::core {

struct ResamplerConfig {
    // Input
    int     inSampleRate   = 48000;
    int     inChannels     = 2;
    int     inFormat       = AV_SAMPLE_FMT_FLTP;  // AVSampleFormat

    // Output
    int     outSampleRate  = 48000;
    int     outChannels    = 2;
    int     outFormat      = AV_SAMPLE_FMT_FLT;   // interleaved float

    bool    operator==(const ResamplerConfig& o) const {
        return inSampleRate  == o.inSampleRate  &&
               inChannels    == o.inChannels    &&
               inFormat      == o.inFormat      &&
               outSampleRate == o.outSampleRate &&
               outChannels   == o.outChannels   &&
               outFormat     == o.outFormat;
    }
};

class AudioResampler {
public:
    AudioResampler();
    ~AudioResampler();

    // Non-copyable
    AudioResampler(const AudioResampler&)            = delete;
    AudioResampler& operator=(const AudioResampler&) = delete;

    // ----- Open / Close ------------------------------------------------------
    bool open(const ResamplerConfig& cfg);
    void close();
    bool isOpen() const { return m_ctx != nullptr; }

    const ResamplerConfig& config() const { return m_cfg; }

    // ----- Conversion --------------------------------------------------------
    /**
     * Convert input PCM data.
     *
     * @param inData     Array of input plane pointers (NULL-terminated for planar,
     *                   single pointer for packed).
     * @param inSamples  Number of samples per channel in input.
     * @return           Interleaved output samples as float (AV_SAMPLE_FMT_FLT).
     *                   Empty on error.
     */
    std::vector<float> convert(const uint8_t** inData, int inSamples);

    /**
     * Flush remaining buffered samples.
     */
    std::vector<float> flush();

    // ----- Speed/tempo -------------------------------------------------------
    /// Set playback speed without pitch change (1.0 = normal, 2.0 = 2x faster).
    void setSpeed(float speed);
    float speed() const { return m_speed; }

    // ----- Delay -------------------------------------------------------------
    /// Compensate for audio/video sync offset (milliseconds, positive = delay audio).
    void setSyncOffsetMs(int offsetMs) { m_syncOffsetMs = offsetMs; }
    int  syncOffsetMs() const          { return m_syncOffsetMs; }

    // ----- Output sample count estimation ------------------------------------
    int outputSamples(int inputSamples) const;

private:
    std::vector<float> drainOutput(int maxSamples);

    SwrContext*     m_ctx          = nullptr;
    ResamplerConfig m_cfg;
    float           m_speed        = 1.0f;
    int             m_syncOffsetMs = 0;

    // Output buffer
    uint8_t**       m_outBuf       = nullptr;
    int             m_outBufSamples = 0;

    static constexpr int k_MaxOutputSamples = 192000; // 4s @ 48kHz
};

} // namespace aurora::core

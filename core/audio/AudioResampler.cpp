/**
 * Aurora Motion Player — Audio Resampler Implementation
 */

#include "AudioResampler.h"

extern "C" {
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
}

#include <cstring>
#include <iostream>

namespace aurora::core {

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
AudioResampler::AudioResampler()  = default;
AudioResampler::~AudioResampler() { close(); }

// ── Open ──────────────────────────────────────────────────────────────────────
bool AudioResampler::open(const ResamplerConfig& cfg) {
    close();
    m_cfg = cfg;

    m_ctx = swr_alloc();
    if (!m_ctx) return false;

    // Input
    av_opt_set_int(m_ctx, "in_sample_rate",   cfg.inSampleRate,  0);
    av_opt_set_int(m_ctx, "in_sample_fmt",    cfg.inFormat,      0);
    av_opt_set_int(m_ctx, "in_channel_count", cfg.inChannels,    0);

    // Output
    av_opt_set_int(m_ctx, "out_sample_rate",   cfg.outSampleRate, 0);
    av_opt_set_int(m_ctx, "out_sample_fmt",    cfg.outFormat,     0);
    av_opt_set_int(m_ctx, "out_channel_count", cfg.outChannels,   0);

    // High-quality resampler
    av_opt_set_int(m_ctx, "filter_length", 16, 0);
    av_opt_set_int(m_ctx, "phase_shift",   10, 0);

    if (swr_init(m_ctx) < 0) {
        swr_free(&m_ctx);
        m_ctx = nullptr;
        return false;
    }

    // Allocate output buffer
    m_outBufSamples = k_MaxOutputSamples;
    av_samples_alloc_array_and_samples(
        &m_outBuf, nullptr,
        cfg.outChannels, m_outBufSamples,
        static_cast<AVSampleFormat>(cfg.outFormat), 0);

    std::cout << "[AudioResampler] "
              << cfg.inSampleRate  << "Hz " << cfg.inChannels  << "ch → "
              << cfg.outSampleRate << "Hz " << cfg.outChannels << "ch\n";
    return true;
}

// ── Close ─────────────────────────────────────────────────────────────────────
void AudioResampler::close() {
    if (m_outBuf) {
        av_freep(&m_outBuf[0]);
        av_freep(&m_outBuf);
        m_outBuf = nullptr;
    }
    if (m_ctx) {
        swr_free(&m_ctx);
        m_ctx = nullptr;
    }
}

// ── Convert ───────────────────────────────────────────────────────────────────
std::vector<float> AudioResampler::convert(const uint8_t** inData, int inSamples) {
    if (!m_ctx || !inData || inSamples <= 0) return {};

    // Account for speed scaling
    int adjustedInSamples = inSamples;
    if (m_speed != 1.0f && m_speed > 0.0f) {
        // Adjust output sample count for tempo (simple rate adjustment)
        // For proper time-stretching, integrate SoundTouch or rubberband
        int newOutRate = static_cast<int>(m_cfg.outSampleRate / m_speed);
        av_opt_set_int(m_ctx, "out_sample_rate", newOutRate, 0);
        swr_init(m_ctx);
    }

    int outSamples = outputSamples(adjustedInSamples);
    if (outSamples > m_outBufSamples) {
        // Reallocate
        av_freep(&m_outBuf[0]);
        av_freep(&m_outBuf);
        m_outBufSamples = outSamples + 1024;
        av_samples_alloc_array_and_samples(
            &m_outBuf, nullptr,
            m_cfg.outChannels, m_outBufSamples,
            static_cast<AVSampleFormat>(m_cfg.outFormat), 0);
    }

    int converted = swr_convert(m_ctx,
                                 m_outBuf,    outSamples,
                                 inData,      adjustedInSamples);
    if (converted < 0) return {};

    return drainOutput(converted);
}

// ── Flush ─────────────────────────────────────────────────────────────────────
std::vector<float> AudioResampler::flush() {
    if (!m_ctx) return {};

    int outSamples = static_cast<int>(
        av_rescale_rnd(swr_get_delay(m_ctx, m_cfg.inSampleRate) + 256,
                        m_cfg.outSampleRate, m_cfg.inSampleRate,
                        AV_ROUND_UP));

    if (outSamples <= 0) return {};

    if (outSamples > m_outBufSamples) {
        av_freep(&m_outBuf[0]);
        av_freep(&m_outBuf);
        m_outBufSamples = outSamples;
        av_samples_alloc_array_and_samples(
            &m_outBuf, nullptr,
            m_cfg.outChannels, m_outBufSamples,
            static_cast<AVSampleFormat>(m_cfg.outFormat), 0);
    }

    int converted = swr_convert(m_ctx, m_outBuf, outSamples, nullptr, 0);
    if (converted <= 0) return {};

    return drainOutput(converted);
}

// ── Drain output buffer ───────────────────────────────────────────────────────
std::vector<float> AudioResampler::drainOutput(int samples) {
    int bytesPerSample = av_get_bytes_per_sample(
        static_cast<AVSampleFormat>(m_cfg.outFormat));
    int totalBytes = samples * m_cfg.outChannels * bytesPerSample;

    std::vector<float> result(samples * m_cfg.outChannels);
    memcpy(result.data(), m_outBuf[0], totalBytes);
    return result;
}

// ── Output sample estimation ──────────────────────────────────────────────────
int AudioResampler::outputSamples(int inputSamples) const {
    if (!m_ctx) return 0;
    return static_cast<int>(
        av_rescale_rnd(
            swr_get_delay(m_ctx, m_cfg.inSampleRate) + inputSamples,
            m_cfg.outSampleRate,
            m_cfg.inSampleRate,
            AV_ROUND_UP)) + 256;
}

// ── Speed ─────────────────────────────────────────────────────────────────────
void AudioResampler::setSpeed(float speed) {
    if (speed <= 0.01f) speed = 0.01f;
    if (speed > 10.0f)  speed = 10.0f;
    m_speed = speed;
}

} // namespace aurora::core

/**
 * Aurora Motion Player — AAudio Output Implementation
 */

#ifdef __ANDROID__

#include "AAudioOutput.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "AuroraAAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace aurora::core {

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
AAudioOutput::AAudioOutput()  = default;
AAudioOutput::~AAudioOutput() { close(); }

// ── Open ──────────────────────────────────────────────────────────────────────
bool AAudioOutput::open(const Config& cfg) {
    m_config = cfg;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        LOGE("Failed to create stream builder: %s", AAudio_convertResultToText(result));
        return false;
    }

    // Configure stream
    AAudioStreamBuilder_setFormat(builder,       AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, cfg.channels);
    AAudioStreamBuilder_setSampleRate(builder,   cfg.sampleRate);
    AAudioStreamBuilder_setDirection(builder,    AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder,  AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setUsage(builder,        AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setContentType(builder,  AAUDIO_CONTENT_TYPE_MOVIE);

    if (cfg.lowLatency) {
        AAudioStreamBuilder_setPerformanceMode(builder,
            AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    }

    if (cfg.bufferCapacity > 0) {
        AAudioStreamBuilder_setBufferCapacityInFrames(builder, cfg.bufferCapacity);
    }

    // Callbacks
    AAudioStreamBuilder_setDataCallback(builder,  dataCallback,  this);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, this);

    // Open stream
    result = AAudioStreamBuilder_openStream(builder, &m_stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        // Fallback: shared mode
        LOGI("Exclusive mode failed, trying shared mode");
        builder = nullptr;
        AAudio_createStreamBuilder(&builder);
        AAudioStreamBuilder_setFormat(builder,       AAUDIO_FORMAT_PCM_FLOAT);
        AAudioStreamBuilder_setChannelCount(builder, cfg.channels);
        AAudioStreamBuilder_setSampleRate(builder,   cfg.sampleRate);
        AAudioStreamBuilder_setDirection(builder,    AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setSharingMode(builder,  AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setUsage(builder,        AAUDIO_USAGE_MEDIA);
        AAudioStreamBuilder_setDataCallback(builder,  dataCallback,  this);
        AAudioStreamBuilder_setErrorCallback(builder, errorCallback, this);
        result = AAudioStreamBuilder_openStream(builder, &m_stream);
        AAudioStreamBuilder_delete(builder);
    }

    if (result != AAUDIO_OK) {
        LOGE("Failed to open AAudio stream: %s", AAudio_convertResultToText(result));
        return false;
    }

    m_sampleRate   = AAudioStream_getSampleRate(m_stream);
    m_channels     = AAudioStream_getChannelCount(m_stream);
    m_bufferFrames = AAudioStream_getBufferCapacityInFrames(m_stream);

    m_open.store(true);
    LOGI("AAudio stream opened: %dHz %dch buffer=%d frames",
         m_sampleRate, m_channels, m_bufferFrames);
    return true;
}

// ── Start / Stop ──────────────────────────────────────────────────────────────
bool AAudioOutput::start() {
    if (!m_open.load()) return false;
    aaudio_result_t result = AAudioStream_requestStart(m_stream);
    if (result != AAUDIO_OK) {
        LOGE("Start failed: %s", AAudio_convertResultToText(result));
        return false;
    }
    m_playing.store(true);
    LOGI("AAudio playback started");
    return true;
}

void AAudioOutput::stop() {
    if (!m_open.load()) return;
    m_playing.store(false);
    AAudioStream_requestStop(m_stream);
    flush();
}

void AAudioOutput::pause() {
    if (!m_open.load()) return;
    m_playing.store(false);
    AAudioStream_requestPause(m_stream);
}

void AAudioOutput::resume() {
    if (!m_open.load()) return;
    m_playing.store(true);
    AAudioStream_requestStart(m_stream);
}

void AAudioOutput::flush() {
    std::lock_guard lock(m_mutex);
    while (!m_queue.empty()) m_queue.pop();
    m_remainder.clear();
    {
        std::lock_guard pl(m_posMutex);
        m_positionUs = 0;
    }
    if (m_stream) AAudioStream_requestFlush(m_stream);
}

// ── Data callback ─────────────────────────────────────────────────────────────
aaudio_data_callback_result_t AAudioOutput::dataCallback(
    AAudioStream* /*stream*/,
    void*         userData,
    void*         audioData,
    int32_t       numFrames)
{
    auto* self    = static_cast<AAudioOutput*>(userData);
    auto* output  = static_cast<float*>(audioData);
    int   needed  = numFrames * self->m_channels;
    int   written = 0;
    float vol     = self->m_volume.load();

    // Drain remainder from previous chunk
    if (!self->m_remainder.empty()) {
        int take = std::min(needed, (int)self->m_remainder.size());
        memcpy(output, self->m_remainder.data(), take * sizeof(float));
        written += take;
        self->m_remainder.erase(self->m_remainder.begin(),
                                 self->m_remainder.begin() + take);
    }

    // Fill from queue
    while (written < needed) {
        std::lock_guard lock(self->m_mutex);
        if (self->m_queue.empty()) {
            // Silence
            memset(output + written, 0, (needed - written) * sizeof(float));
            written = needed;
            if (self->m_underrunCb) self->m_underrunCb();
            break;
        }

        auto& chunk = self->m_queue.front();
        int available = static_cast<int>(chunk.samples.size());
        int take      = std::min(needed - written, available);

        memcpy(output + written, chunk.samples.data(), take * sizeof(float));
        written += take;

        if (take < available) {
            // Leftover — store in remainder
            self->m_remainder.assign(
                chunk.samples.begin() + take,
                chunk.samples.end());
        }

        {
            std::lock_guard pl(self->m_posMutex);
            self->m_positionUs = chunk.pts;
        }
        self->m_queue.pop();
    }

    // Apply volume
    if (vol < 0.999f) {
        for (int i = 0; i < needed; ++i) output[i] *= vol;
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// ── Error callback ────────────────────────────────────────────────────────────
void AAudioOutput::errorCallback(
    AAudioStream* /*stream*/,
    void*         userData,
    aaudio_result_t error)
{
    auto* self = static_cast<AAudioOutput*>(userData);
    LOGE("AAudio error: %s", AAudio_convertResultToText(error));
    self->handleError(error);
}

void AAudioOutput::handleError(aaudio_result_t error) {
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        // Device disconnected (headphone unplugged, etc.)
        LOGI("Audio device disconnected, restarting...");
        restartStream();
    }
}

bool AAudioOutput::restartStream() {
    close();
    if (!open(m_config)) return false;
    return start();
}

// ── Push chunk ────────────────────────────────────────────────────────────────
bool AAudioOutput::pushChunk(AudioChunkAndroid chunk) {
    std::lock_guard lock(m_mutex);
    if (static_cast<int>(m_queue.size()) >= k_MaxChunks) return false;
    m_queue.push(std::move(chunk));
    return true;
}

// ── Position & latency ────────────────────────────────────────────────────────
int64_t AAudioOutput::positionUs() const {
    std::lock_guard lock(m_posMutex);
    return m_positionUs;
}

int AAudioOutput::latencyMs() const {
    if (!m_stream) return 0;
    int32_t xRunCount = AAudioStream_getXRunCount(m_stream);
    (void)xRunCount;
    // Estimate from buffer size and sample rate
    int frames = AAudioStream_getBufferSizeInFrames(m_stream);
    return (frames * 1000) / m_sampleRate;
}

// ── Close ─────────────────────────────────────────────────────────────────────
void AAudioOutput::close() {
    if (!m_open.load()) return;
    m_playing.store(false);
    m_open.store(false);
    if (m_stream) {
        AAudioStream_requestStop(m_stream);
        AAudioStream_close(m_stream);
        m_stream = nullptr;
    }
    LOGI("AAudio stream closed");
}

} // namespace aurora::core

#endif // __ANDROID__

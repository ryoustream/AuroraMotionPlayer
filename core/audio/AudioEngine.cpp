#include "AudioEngine.h"
#include <stdexcept>
#include <thread>
#include <chrono>

#ifdef _WIN32
#  define AURORA_AUDIO_WASAPI 1
#endif

namespace aurora::audio {

AudioEngine::AudioEngine(AudioEngineConfig cfg)
    : m_config(std::move(cfg))
{}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
#ifdef AURORA_AUDIO_WASAPI
    // WASAPI initialization (Windows)
    // Full implementation uses IMMDeviceEnumerator / IAudioClient
    m_running = true;
    return true;
#else
    m_running = true;
    return true;
#endif
}

void AudioEngine::shutdown() {
    m_running = false;
}

void AudioEngine::push(AudioBufferPtr buf) {
    if (!m_running || !buf) return;
    // Feed buffer to platform audio output
    // In full implementation: enqueue to lock-free ring buffer consumed by outputLoop
}

void AudioEngine::flush() {
    // Clear internal audio queue and platform buffer
}

void AudioEngine::pause() {
    m_paused = true;
}

void AudioEngine::resume() {
    m_paused = false;
}

void AudioEngine::setVolume(float vol) noexcept {
    m_config.volume = vol;
}

void AudioEngine::setBalance(float bal) noexcept {
    m_config.balance = bal;
}

void AudioEngine::setOutputMode(AudioOutputMode mode) {
    m_config.mode = mode;
}

double AudioEngine::latencyMs() const noexcept {
    return static_cast<double>(m_config.bufferMs);
}

std::vector<std::string> AudioEngine::enumerateDevices() {
    std::vector<std::string> devices;
    devices.push_back("Default");
#ifdef AURORA_AUDIO_WASAPI
    devices.push_back("WASAPI Default");
    devices.push_back("SPDIF");
    devices.push_back("HDMI");
#endif
    return devices;
}

} // namespace aurora::audio

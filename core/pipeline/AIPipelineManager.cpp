/**
 * Aurora Motion Player — AI Processing Pipeline Implementation
 */

#include "AIPipelineManager.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace aurora::core {

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<float, std::milli>;

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
AIPipelineManager::AIPipelineManager() {
    m_hdrEngine   = std::make_unique<HDREngine>();
    m_auroraFlow  = std::make_unique<AuroraFlow>();
}

AIPipelineManager::~AIPipelineManager() {
    stop();
}

// ── Configure ─────────────────────────────────────────────────────────────────
void AIPipelineManager::configure(const PipelineConfig& config) {
    if (m_state.load() == PipelineState::Running) {
        std::cerr << "[Pipeline] Cannot reconfigure while running\n";
        return;
    }
    m_config = config;

    // Apply HDR config
    if (m_config.hdrEnabled) {
        HDRConfig hdrCfg;
        hdrCfg.enabled = true;
        if      (m_config.toneMappingMode == "BT2390")  hdrCfg.tonemapMode = ToneMappingMode::BT2390;
        else if (m_config.toneMappingMode == "Mobius")  hdrCfg.tonemapMode = ToneMappingMode::Mobius;
        else if (m_config.toneMappingMode == "ACES")    hdrCfg.tonemapMode = ToneMappingMode::ACES;
        else if (m_config.toneMappingMode == "Reinhard") hdrCfg.tonemapMode = ToneMappingMode::Reinhard;
        m_hdrEngine->configure(hdrCfg);
    }

    // Apply AuroraFlow config
    if (m_config.interpEnabled) {
        AuroraFlowConfig afCfg;
        afCfg.modelName  = m_config.interpModel;
        afCfg.targetFPS  = m_config.targetFPS;
        m_auroraFlow->configure(afCfg);
    }
}

// ── Start / Stop ──────────────────────────────────────────────────────────────
bool AIPipelineManager::start() {
    PipelineState expected = PipelineState::Idle;
    if (!m_state.compare_exchange_strong(expected, PipelineState::Running)) {
        if (m_state.load() == PipelineState::Paused) {
            resume();
            return true;
        }
        return false;
    }

    m_processThread = std::thread(&AIPipelineManager::processingLoop, this);
    std::cout << "[Pipeline] Started\n";
    return true;
}

void AIPipelineManager::pause() {
    PipelineState expected = PipelineState::Running;
    m_state.compare_exchange_strong(expected, PipelineState::Paused);
}

void AIPipelineManager::resume() {
    PipelineState expected = PipelineState::Paused;
    if (m_state.compare_exchange_strong(expected, PipelineState::Running)) {
        m_inputCV.notify_all();
    }
}

void AIPipelineManager::stop() {
    PipelineState prev = m_state.exchange(PipelineState::Idle);
    if (prev == PipelineState::Idle) return;

    m_inputCV.notify_all();
    if (m_processThread.joinable()) m_processThread.join();

    std::lock_guard lock(m_inputMutex);
    while (!m_inputQueue.empty()) m_inputQueue.pop();
    m_prevFrame.reset();

    std::cout << "[Pipeline] Stopped\n";
}

void AIPipelineManager::flush() {
    std::lock_guard lock(m_inputMutex);
    while (!m_inputQueue.empty()) m_inputQueue.pop();
    m_prevFrame.reset();
}

// ── Push frame ────────────────────────────────────────────────────────────────
bool AIPipelineManager::pushFrame(std::shared_ptr<VideoFrame> frame) {
    std::lock_guard lock(m_inputMutex);
    if (static_cast<int>(m_inputQueue.size()) >= m_config.decodeQueueSize) {
        // Drop oldest frame (back-pressure)
        m_inputQueue.pop();
        std::lock_guard sl(m_statsMutex);
        ++m_stats.droppedFrames;
    }
    m_inputQueue.push(std::move(frame));
    m_inputCV.notify_one();
    return true;
}

// ── Processing loop ───────────────────────────────────────────────────────────
void AIPipelineManager::processingLoop() {
    while (true) {
        // Wait for frame or stop
        std::shared_ptr<VideoFrame> frame;
        {
            std::unique_lock lock(m_inputMutex);
            m_inputCV.wait(lock, [this] {
                return !m_inputQueue.empty() ||
                       m_state.load() == PipelineState::Idle;
            });

            if (m_inputQueue.empty()) break; // stop signal
            frame = std::move(m_inputQueue.front());
            m_inputQueue.pop();
        }

        // Handle pause
        while (m_state.load() == PipelineState::Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (m_state.load() == PipelineState::Idle) break;

        auto t0 = Clock::now();

        // --- Denoise ---------------------------------------------------------
        if (m_config.denoiseEnabled) {
            auto td = Clock::now();
            frame = runDenoise(frame);
            float ms = Ms(Clock::now() - td).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.denoiseLatencyMs = 0.9f * m_stats.denoiseLatencyMs + 0.1f * ms;
        }

        // --- Upscale ---------------------------------------------------------
        if (m_config.upscaleEnabled) {
            auto tu = Clock::now();
            frame = runUpscale(frame);
            float ms = Ms(Clock::now() - tu).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.upscaleLatencyMs = 0.9f * m_stats.upscaleLatencyMs + 0.1f * ms;
        }

        // --- Frame interpolation ---------------------------------------------
        if (m_config.interpEnabled && m_prevFrame) {
            bool scenecut = m_sceneChanged.exchange(false);
            if (!scenecut || !m_config.adaptiveMotion) {
                auto ti = Clock::now();
                runInterpolate(m_prevFrame, frame);
                float ms = Ms(Clock::now() - ti).count();
                std::lock_guard sl(m_statsMutex);
                m_stats.interpLatencyMs = 0.9f * m_stats.interpLatencyMs + 0.1f * ms;
                ++m_stats.interpolatedFrames;
            }
        }

        // --- Tone mapping ----------------------------------------------------
        if (m_config.hdrEnabled) {
            auto tt = Clock::now();
            frame = runToneMap(frame);
            float ms = Ms(Clock::now() - tt).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.toneMappingLatencyMs = 0.9f * m_stats.toneMappingLatencyMs + 0.1f * ms;
        }

        // --- Output ----------------------------------------------------------
        m_prevFrame = frame;
        if (m_outputCb) {
            try {
                m_outputCb(frame);
            } catch (const std::exception& e) {
                std::cerr << "[Pipeline] Output callback threw: " << e.what() << "\n";
            }
        }

        float totalMs = Ms(Clock::now() - t0).count();
        {
            std::lock_guard sl(m_statsMutex);
            m_stats.totalLatencyMs = 0.9f * m_stats.totalLatencyMs + 0.1f * totalMs;
            if (totalMs > 0.0f)
                m_stats.throughputFPS = 0.9f * m_stats.throughputFPS + 0.1f * (1000.0f / totalMs);
        }
    }
}

// ── Stage implementations (stubs — integrate real backends) ──────────────────
std::shared_ptr<VideoFrame> AIPipelineManager::runDenoise(std::shared_ptr<VideoFrame> frame) {
    // TODO: integrate NCNN-based spatial denoiser
    // For now, pass-through
    return frame;
}

std::shared_ptr<VideoFrame> AIPipelineManager::runUpscale(std::shared_ptr<VideoFrame> frame) {
    // TODO: route to UpscalerFactory based on m_config.upscalerName
    // auto* up = m_upscalerFactory.create(m_config.upscalerName);
    // return up->upscale(frame, m_config.upscaleScale);
    return frame;
}

void AIPipelineManager::runInterpolate(std::shared_ptr<VideoFrame> prev,
                                        std::shared_ptr<VideoFrame> curr) {
    // TODO: route to AuroraFlow
    // m_auroraFlow->interpolate(prev, curr, outputCb);
    (void)prev; (void)curr;
}

std::shared_ptr<VideoFrame> AIPipelineManager::runToneMap(std::shared_ptr<VideoFrame> frame) {
    // TODO: route to HDREngine
    // return m_hdrEngine->process(frame);
    return frame;
}

// ── Scene change notification ─────────────────────────────────────────────────
void AIPipelineManager::onSceneChange() {
    m_sceneChanged.store(true);
}

// ── Stats ─────────────────────────────────────────────────────────────────────
PipelineStats AIPipelineManager::stats() const {
    std::lock_guard lock(m_statsMutex);
    return m_stats;
}

} // namespace aurora::core

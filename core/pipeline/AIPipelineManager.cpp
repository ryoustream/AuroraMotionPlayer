/**
 * Aurora Motion Player — AI Processing Pipeline Implementation
 * Session 13 Fix: Corrected all type names to match actual headers.
 * HDRConfig→DisplayCapabilities, ToneMappingMode→ToneMappingAlgorithm,
 * AuroraFlowConfig→InterpolationConfig, configure()→init()/setConfig()
 */

#include "AIPipelineManager.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace aurora::core {

// Type aliases matching the ACTUAL headers
using VFPtr  = aurora::video::VideoFramePtr;
using Clock  = std::chrono::steady_clock;
using Ms     = std::chrono::duration<float, std::milli>;

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
AIPipelineManager::AIPipelineManager() {
    m_hdrEngine  = std::make_unique<aurora::hdr::HDREngine>();
    m_auroraFlow = std::make_unique<aurora::interpolation::AuroraFlow>();
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

    // Apply HDR config using the actual HDREngine API
    if (m_config.hdrEnabled && m_hdrEngine) {
        aurora::hdr::DisplayCapabilities disp;
        disp.isHDRCapable = true;
        disp.maxLuminance = 1000.0f;
        m_hdrEngine->init(disp);

        // Map string to actual ToneMappingAlgorithm enum
        using Algo = aurora::hdr::ToneMappingAlgorithm;
        Algo algo = Algo::BT2390;
        if      (m_config.toneMappingMode == "Mobius")   algo = Algo::Mobius;
        else if (m_config.toneMappingMode == "ACES")     algo = Algo::ACES;
        else if (m_config.toneMappingMode == "Reinhard") algo = Algo::Reinhard;
        m_hdrEngine->setToneMapper(algo);
    }

    // Apply AuroraFlow config using InterpolationConfig (the actual type)
    if (m_config.interpEnabled && m_auroraFlow) {
        aurora::interpolation::InterpolationConfig icfg;
        icfg.sourceFPS  = 24.0f;               // will be updated per-file
        icfg.targetFPS  = m_config.targetFPS;
        icfg.sceneDetect = m_config.adaptiveMotion;
        // Map model name string to enum
        using Model = aurora::interpolation::InterpolationModel;
        icfg.model = Model::RIFE;
        if      (m_config.interpModel == "IFRNet")  icfg.model = Model::IFRNet;
        else if (m_config.interpModel == "FILM")    icfg.model = Model::FILM;
        else if (m_config.interpModel == "GMFlow")  icfg.model = Model::GMFlow;
        m_auroraFlow->setConfig(icfg);
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
bool AIPipelineManager::pushFrame(aurora::video::VideoFramePtr frame) {
    std::lock_guard lock(m_inputMutex);
    if (static_cast<int>(m_inputQueue.size()) >= m_config.decodeQueueSize) {
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
        aurora::video::VideoFramePtr frame;
        {
            std::unique_lock lock(m_inputMutex);
            m_inputCV.wait(lock, [this] {
                return !m_inputQueue.empty() ||
                       m_state.load() == PipelineState::Idle;
            });
            if (m_inputQueue.empty()) break;
            frame = std::move(m_inputQueue.front());
            m_inputQueue.pop();
        }

        while (m_state.load() == PipelineState::Paused)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (m_state.load() == PipelineState::Idle) break;

        auto t0 = Clock::now();

        if (m_config.denoiseEnabled) {
            auto td = Clock::now();
            frame = runDenoise(frame);
            float ms = Ms(Clock::now() - td).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.denoiseLatencyMs = 0.9f * m_stats.denoiseLatencyMs + 0.1f * ms;
        }

        if (m_config.upscaleEnabled) {
            auto tu = Clock::now();
            frame = runUpscale(frame);
            float ms = Ms(Clock::now() - tu).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.upscaleLatencyMs = 0.9f * m_stats.upscaleLatencyMs + 0.1f * ms;
        }

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

        if (m_config.hdrEnabled) {
            auto tt = Clock::now();
            frame = runToneMap(frame);
            float ms = Ms(Clock::now() - tt).count();
            std::lock_guard sl(m_statsMutex);
            m_stats.toneMappingLatencyMs = 0.9f * m_stats.toneMappingLatencyMs + 0.1f * ms;
        }

        m_prevFrame = frame;
        if (m_outputCb) {
            try { m_outputCb(frame); }
            catch (const std::exception& e) {
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

// ── Stage implementations (stubs — integrate real backends when libs present) ─
aurora::video::VideoFramePtr
AIPipelineManager::runDenoise(aurora::video::VideoFramePtr frame) {
    return frame; // pass-through until NCNN denoiser is wired
}

aurora::video::VideoFramePtr
AIPipelineManager::runUpscale(aurora::video::VideoFramePtr frame) {
    return frame; // pass-through until UpscalerFactory is wired
}

void AIPipelineManager::runInterpolate(aurora::video::VideoFramePtr prev,
                                        aurora::video::VideoFramePtr curr) {
    // AuroraFlow interpolates between prev and curr and calls the output callback
    if (m_auroraFlow) {
        m_auroraFlow->push(prev);
        m_auroraFlow->push(curr);
    } else {
        // fallback: emit curr directly
        if (m_outputCb) m_outputCb(curr);
    }
}

aurora::video::VideoFramePtr
AIPipelineManager::runToneMap(aurora::video::VideoFramePtr frame) {
    if (m_hdrEngine && frame) {
        aurora::hdr::HDRMetadata meta; // uses defaults (HDR10)
        return m_hdrEngine->process(frame, meta);
    }
    return frame;
}

// ── Scene change ──────────────────────────────────────────────────────────────
void AIPipelineManager::onSceneChange() {
    m_sceneChanged.store(true);
}

// ── Stats ─────────────────────────────────────────────────────────────────────
PipelineStats AIPipelineManager::stats() const {
    std::lock_guard lock(m_statsMutex);
    return m_stats;
}

} // namespace aurora::core

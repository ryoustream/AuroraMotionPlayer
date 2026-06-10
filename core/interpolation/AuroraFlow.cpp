#include "AuroraFlow.h"
#include "RIFEInterpolator.h"
#include "IFRNetInterpolator.h"
#include "FILMInterpolator.h"
#include "GMFlowInterpolator.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <filesystem>

namespace aurora::interpolation {

// ── PIMPL ─────────────────────────────────────────────────────────────────────
struct AuroraFlow::Impl {
    std::unique_ptr<RIFEInterpolator>    rife;
    std::unique_ptr<IFRNetInterpolator>  ifrnet;
    std::unique_ptr<FILMInterpolator>    film;
    std::unique_ptr<GMFlowInterpolator>  gmflow;

    InterpolationModel activeModel = InterpolationModel::RIFE;
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
AuroraFlow::AuroraFlow()
    : m_impl(std::make_unique<Impl>())
{}

AuroraFlow::~AuroraFlow() {
    shutdown();
}

// ── init ──────────────────────────────────────────────────────────────────────
bool AuroraFlow::init(const InterpolationConfig& cfg) {
    m_cfg = cfg;
    m_impl->activeModel = cfg.model;

    bool ok = false;
    switch (cfg.model) {
    case InterpolationModel::RIFE:
        m_impl->rife = std::make_unique<RIFEInterpolator>();
        ok = m_impl->rife->init(cfg);
        break;
    case InterpolationModel::IFRNet:
        m_impl->ifrnet = std::make_unique<IFRNetInterpolator>();
        ok = m_impl->ifrnet->init(cfg);
        break;
    case InterpolationModel::FILM:
        m_impl->film = std::make_unique<FILMInterpolator>();
        ok = m_impl->film->init(cfg);
        break;
    case InterpolationModel::GMFlow:
        m_impl->gmflow = std::make_unique<GMFlowInterpolator>();
        ok = m_impl->gmflow->init(cfg);
        break;
    }

    if (ok) m_running = true;
    return ok;
}

void AuroraFlow::shutdown() {
    m_running = false;
    m_prevFrame.reset();
    m_impl->rife.reset();
    m_impl->ifrnet.reset();
    m_impl->film.reset();
    m_impl->gmflow.reset();
}

// ── push ──────────────────────────────────────────────────────────────────────
void AuroraFlow::push(video::VideoFramePtr frame) {
    if (!m_running || !frame) return;

    ++m_stats.framesIn;

    if (!m_prevFrame) {
        // First frame — just store and emit as-is
        m_prevFrame = frame;
        if (m_outputCb) m_outputCb(frame);
        return;
    }

    // Check for scene cut — skip interpolation if cut detected
    if (m_cfg.sceneDetect && detectSceneCut(m_prevFrame, frame)) {
        ++m_stats.sceneCuts;
        if (m_outputCb) m_outputCb(m_prevFrame);
        m_prevFrame = frame;
        if (m_outputCb) m_outputCb(frame);
        return;
    }

    // Compute interpolation multiplier (e.g. 24→60 = 2.5x ≈ generate 1-2 in-between frames)
    float multiplier = computeMultiplier();
    int   steps      = static_cast<int>(std::round(multiplier)) - 1;

    // Emit previous frame
    if (m_outputCb) m_outputCb(m_prevFrame);
    ++m_stats.framesOut;

    // Generate intermediate frames
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps + 1);
        interpolateFrames(m_prevFrame, frame, t);
    }

    m_prevFrame = frame;
}

// ── interpolateFrames ─────────────────────────────────────────────────────────
void AuroraFlow::interpolateFrames(video::VideoFramePtr f0,
                                   video::VideoFramePtr f1,
                                   float t)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    video::VideoFramePtr result;

    switch (m_impl->activeModel) {
    case InterpolationModel::RIFE:
        if (m_impl->rife) result = m_impl->rife->interpolate(f0, f1, t);
        break;
    case InterpolationModel::IFRNet:
        if (m_impl->ifrnet) result = m_impl->ifrnet->interpolate(f0, f1, t);
        break;
    case InterpolationModel::FILM:
        if (m_impl->film) result = m_impl->film->interpolate(f0, f1, t);
        break;
    case InterpolationModel::GMFlow:
        if (m_impl->gmflow) result = m_impl->gmflow->interpolate(f0, f1, t);
        break;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_stats.avgInferenceMs = m_stats.avgInferenceMs * 0.9 + ms * 0.1;

    if (result) {
        ++m_stats.framesOut;
        // Compute interpolated PTS
        int64_t pts = f0->pts() + static_cast<int64_t>((f1->pts() - f0->pts()) * t);
        result->setPts(pts);
        result->setTimeBase(f0->timeBase());
        if (m_outputCb) m_outputCb(result);
    }
}

// ── detectSceneCut ────────────────────────────────────────────────────────────
bool AuroraFlow::detectSceneCut(video::VideoFramePtr f0,
                                video::VideoFramePtr f1)
{
    if (!f0 || !f1) return false;
    if (f0->width() != f1->width() || f0->height() != f1->height()) return true;

    // Fast SAD-based scene detection on luma plane (Y channel)
    const uint8_t* y0 = f0->data(0);
    const uint8_t* y1 = f1->data(0);
    if (!y0 || !y1) return false;

    int w = f0->width(), h = f0->height();
    int step = std::max(1, (w * h) / 4096); // Sample ~4096 pixels
    double sad = 0.0;
    int count = 0;

    for (int i = 0; i < w * h; i += step) {
        sad += std::abs(static_cast<int>(y0[i]) - static_cast<int>(y1[i]));
        ++count;
    }

    double normalizedSAD = (count > 0) ? (sad / count / 255.0) : 0.0;
    return normalizedSAD > m_cfg.sceneThreshold;
}

// ── computeMultiplier ─────────────────────────────────────────────────────────
float AuroraFlow::computeMultiplier() const noexcept {
    if (m_cfg.sourceFPS <= 0.0f) return 2.0f;
    return m_cfg.targetFPS / m_cfg.sourceFPS;
}

void AuroraFlow::flush() {
    if (m_prevFrame && m_outputCb) {
        m_outputCb(m_prevFrame);
    }
    m_prevFrame.reset();
}

std::vector<std::string> AuroraFlow::availableModels(const std::string& modelDir) {
    std::vector<std::string> models;
    if (modelDir.empty()) return models;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
            if (entry.path().extension() == ".param" ||
                entry.path().extension() == ".onnx") {
                models.push_back(entry.path().string());
            }
        }
    } catch (...) {}
    return models;
}

float AuroraFlow::inputFPS()  const noexcept { return m_cfg.sourceFPS; }
float AuroraFlow::outputFPS() const noexcept { return m_cfg.targetFPS; }

} // namespace aurora::interpolation

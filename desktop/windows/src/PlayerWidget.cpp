#include "PlayerWidget.h"

#include "decoder/FFmpegDecoder.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/OpenGLRenderer.h"
#include "interpolation/AuroraFlow.h"
#include "upscaler/UpscalerFactory.h"
#include "hdr/HDREngine.h"
#include "audio/AudioEngine.h"
#include "subtitle/SubtitleEngine.h"
#include "benchmark/BenchmarkSystem.h"
#include "scene/SceneDetector.h"

#include <QResizeEvent>
#include <QPainter>

PlayerWidget::PlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setMouseTracking(true);
    initPipeline();
}

PlayerWidget::~PlayerWidget() {
    stop();
}

void PlayerWidget::initPipeline() {
    // Decoder
    aurora::decoder::DecoderConfig dcfg;
    dcfg.hwAccel = aurora::decoder::HWAccelType::D3D11VA;
    m_decoder = std::make_unique<aurora::decoder::FFmpegDecoder>(dcfg);
    m_decoder->setVideoCallback([this](aurora::video::VideoFramePtr f) { onVideoFrame(std::move(f)); });
    m_decoder->setErrorCallback([this](const std::string& e) { onError(e); });

    // Renderer (Vulkan primary, OpenGL fallback)
    auto* vk = new aurora::renderer::VulkanRenderer();
    aurora::renderer::RendererConfig rcfg;
    rcfg.width  = width();
    rcfg.height = height();
    if (!vk->init(reinterpret_cast<void*>(winId()), rcfg)) {
        delete vk;
        auto* gl = new aurora::renderer::OpenGLRenderer();
        gl->init(reinterpret_cast<void*>(winId()), rcfg);
        m_renderer.reset(gl);
    } else {
        m_renderer.reset(vk);
    }

    // AI Frame Interpolation
    aurora::interpolation::InterpolationConfig icfg;
    icfg.model    = aurora::interpolation::InterpolationModel::RIFE;
    icfg.quality  = aurora::interpolation::InterpolationQuality::Balanced;
    icfg.backend  = aurora::interpolation::InferenceBackend::NCNN;
    icfg.modelPath = "models/rife";
    icfg.sourceFPS = 24.0f;
    icfg.targetFPS = 60.0f;
    m_flow = std::make_unique<aurora::interpolation::AuroraFlow>();
    m_flow->setOutputCallback([this](aurora::video::VideoFramePtr f) {
        m_renderer->renderFrame(f);
        m_renderer->present();
        m_bench->onFrameRendered();
    });

    // HDR
    m_hdr = std::make_unique<aurora::hdr::HDREngine>();
    aurora::hdr::DisplayCapabilities disp;
    disp.maxLuminance = 400.0f;
    disp.isHDRCapable = false;
    m_hdr->init(disp);

    // Audio
    aurora::audio::AudioEngineConfig acfg;
    m_audio = std::make_unique<aurora::audio::AudioEngine>(acfg);
    m_audio->init();

    // Subtitle
    m_subtitle = std::make_unique<aurora::subtitle::SubtitleEngine>();

    // Benchmark
    m_bench = std::make_unique<aurora::benchmark::BenchmarkSystem>();

    // Scene detector
    m_scene = std::make_unique<aurora::scene::SceneDetector>();

    // Upscaler (default RealESRGAN x2)
    m_upscaler = aurora::upscaler::UpscalerFactory::create(
        aurora::upscaler::UpscaleModel::RealESRGAN);
}

bool PlayerWidget::open(const QString& path) {
    stop();
    if (!m_decoder->open(path.toStdString())) return false;
    const auto& meta = m_decoder->metadata();
    if (auto* vs = meta.primaryVideo()) {
        m_flow->init(aurora::interpolation::InterpolationConfig{
            .sourceFPS = static_cast<float>(vs->frameRate),
            .targetFPS = 60.0f,
        });
        emit durationChanged(meta.duration);
    }
    m_bench->start();
    m_decoder->play();
    m_playing = true;
    emit playbackStarted();
    return true;
}

void PlayerWidget::stop() {
    if (m_decoder) m_decoder->close();
    if (m_flow)    m_flow->flush();
    if (m_bench)   m_bench->stop();
    m_playing = false;
    emit playbackStopped();
}

void PlayerWidget::togglePlayPause() {
    if (!m_decoder) return;
    if (m_playing) {
        m_decoder->pause();
        m_playing = false;
        emit playbackPaused();
    } else {
        m_decoder->play();
        m_playing = true;
        emit playbackStarted();
    }
}

void PlayerWidget::seek(double seconds) {
    if (m_decoder) m_decoder->seek(seconds);
}

void PlayerWidget::seekRelative(double delta) {
    seek(position() + delta);
}

void PlayerWidget::setVolume(int percent) {
    m_volume = qBound(0, percent, 100);
    if (m_audio) m_audio->setVolume(m_volume / 100.0f);
}

double PlayerWidget::position() const noexcept {
    return m_decoder ? m_decoder->position() : 0.0;
}

double PlayerWidget::duration() const noexcept {
    return m_decoder ? m_decoder->duration() : 0.0;
}

void PlayerWidget::onVideoFrame(aurora::video::VideoFramePtr frame) {
    m_bench->onFrameDecoded();
    // HDR tone-mapping if needed
    aurora::hdr::HDRMetadata hdrMeta;
    hdrMeta.format = m_hdr->detectFormat(*frame);
    frame = m_hdr->process(frame, hdrMeta);
    // Push to interpolation pipeline
    m_flow->push(frame);
    emit positionChanged(frame->timestampSeconds());
}

void PlayerWidget::onAudioBuffer(aurora::audio::AudioBufferPtr buf) {
    if (m_audio) m_audio->push(buf);
}

void PlayerWidget::onError(const std::string& msg) {
    emit errorOccurred(QString::fromStdString(msg));
}

void PlayerWidget::paintEvent(QPaintEvent*) {
    // Rendering is handled by native Vulkan/OpenGL renderer
}

void PlayerWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (m_renderer) m_renderer->resize(e->size().width(), e->size().height());
}

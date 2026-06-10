#include "SettingsDialog.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QLabel>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings — Aurora Player");
    setMinimumSize(500, 400);

    auto* mainLay = new QVBoxLayout(this);
    auto* tabs    = new QTabWidget;

    // ── AI Tab ──────────────────────────────────────────────────────────────
    auto* aiW   = new QWidget;
    auto* aiLay = new QFormLayout(aiW);

    auto* modelCombo = new QComboBox;
    modelCombo->addItems({"RIFE", "IFRNet", "FILM", "GMFlow"});

    auto* qualityCombo = new QComboBox;
    qualityCombo->addItems({"Fast", "Balanced", "High", "Ultra"});
    qualityCombo->setCurrentIndex(1);

    auto* backendCombo = new QComboBox;
    backendCombo->addItems({"NCNN (Vulkan)", "ONNX Runtime", "TensorRT"});

    auto* targetFPS = new QComboBox;
    targetFPS->addItems({"60 FPS", "120 FPS", "144 FPS"});

    auto* sceneDetect = new QCheckBox("Auto scene cut detection");
    sceneDetect->setChecked(true);

    auto* upscaleModel = new QComboBox;
    upscaleModel->addItems({"Disabled", "RealESRGAN x2", "RealESRGAN x4", "Anime4K", "SPAN", "FSRCNN"});

    aiLay->addRow("Interpolation Model:", modelCombo);
    aiLay->addRow("Quality:", qualityCombo);
    aiLay->addRow("Backend:", backendCombo);
    aiLay->addRow("Target FPS:", targetFPS);
    aiLay->addRow("", sceneDetect);
    aiLay->addRow("Upscaler:", upscaleModel);

    // ── Video Tab ────────────────────────────────────────────────────────────
    auto* vidW   = new QWidget;
    auto* vidLay = new QFormLayout(vidW);

    auto* rendererCombo = new QComboBox;
    rendererCombo->addItems({"Vulkan (Recommended)", "DirectX 12", "DirectX 11", "OpenGL"});

    auto* hwDecodeCombo = new QComboBox;
    hwDecodeCombo->addItems({"Auto", "D3D11VA", "NVDEC", "QSV", "Software"});

    auto* hdrCheck = new QCheckBox("Enable HDR output (requires HDR display)");
    auto* tonemap  = new QComboBox;
    tonemap->addItems({"BT.2390", "Mobius", "ACES", "Reinhard"});

    vidLay->addRow("Renderer:", rendererCombo);
    vidLay->addRow("HW Decode:", hwDecodeCombo);
    vidLay->addRow("", hdrCheck);
    vidLay->addRow("Tone Mapping:", tonemap);

    // ── Audio Tab ────────────────────────────────────────────────────────────
    auto* audW   = new QWidget;
    auto* audLay = new QFormLayout(audW);
    auto* deviceCombo = new QComboBox;
    deviceCombo->addItems({"Default", "WASAPI", "SPDIF", "HDMI"});
    auto* passthroughCheck = new QCheckBox("Enable bitstream passthrough (HDMI/SPDIF)");
    audLay->addRow("Output Device:", deviceCombo);
    audLay->addRow("", passthroughCheck);

    tabs->addTab(aiW,  "AI / Interpolation");
    tabs->addTab(vidW, "Video");
    tabs->addTab(audW, "Audio");

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLay->addWidget(tabs);
    mainLay->addWidget(btns);
}

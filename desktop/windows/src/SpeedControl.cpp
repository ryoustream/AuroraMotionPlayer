#include "SpeedControl.h"
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>

const QVector<double> SpeedControl::kPresets = {
    0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0
};

static const char* kStyle = R"(
QWidget { background: transparent; color: #C0C0E0; font-size: 11px; }
QSlider::groove:horizontal {
    background: #1E1E32; border-radius: 3px; height: 4px;
}
QSlider::handle:horizontal {
    background: #7070CC; border-radius: 5px;
    width: 10px; height: 10px; margin: -3px 0;
}
QSlider::sub-page:horizontal { background: #5050AA; border-radius: 3px; }
QComboBox {
    background: #1A1A2A; border: 1px solid #2A2A4A;
    border-radius: 4px; padding: 2px 6px;
}
QPushButton {
    background: #1A1A2A; border: 1px solid #2A2A4A;
    border-radius: 4px; padding: 3px 8px; color: #8888CC;
}
QPushButton:hover { background: #2A2A4A; color: #C0C0FF; }
)";

SpeedControl::SpeedControl(QWidget* parent) : QWidget(parent)
{
    setStyleSheet(kStyle);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(6, 4, 6, 4);
    vl->setSpacing(4);

    // Row 1: label + slider
    auto* row1 = new QHBoxLayout;
    m_label = new QLabel("1.00×", this);
    m_label->setMinimumWidth(46);
    row1->addWidget(m_label);

    m_slider = new QSlider(Qt::Horizontal, this);
    // Range: 5 = 0.05× … 200 = 4.0×
    m_slider->setRange(5, 200);
    m_slider->setValue(100);
    m_slider->setSingleStep(5);
    m_slider->setPageStep(25);
    m_slider->setTickInterval(25);
    row1->addWidget(m_slider, 1);

    m_resetBtn = new QPushButton("1×", this);
    m_resetBtn->setFixedWidth(28);
    row1->addWidget(m_resetBtn);

    vl->addLayout(row1);

    // Row 2: presets
    auto* row2 = new QHBoxLayout;
    QLabel* presetsLabel = new QLabel("Preset:", this);
    row2->addWidget(presetsLabel);

    m_preset = new QComboBox(this);
    for (double v : kPresets)
        m_preset->addItem(QString("%1×").arg(v, 0, 'f', 2), v);
    m_preset->setCurrentIndex(3); // 1.00×
    row2->addWidget(m_preset, 1);
    vl->addLayout(row2);

    connect(m_slider,  &QSlider::valueChanged,
            this,      &SpeedControl::onSliderMoved);
    connect(m_preset,  qOverload<int>(&QComboBox::currentIndexChanged),
            this,      &SpeedControl::onPresetSelected);
    connect(m_resetBtn, &QPushButton::clicked, this, &SpeedControl::onReset);
}

void SpeedControl::setSpeed(double speed)
{
    speed = qBound(0.05, speed, 4.0);
    if (qAbs(speed - m_speed) < 0.001) return;
    m_speed = speed;

    // Update slider without triggering signal loop
    QSignalBlocker sb1(m_slider);
    m_slider->setValue(speedToSlider(speed));

    // Update combo
    QSignalBlocker sb2(m_preset);
    int bestIdx = 3;
    double bestDiff = 1e9;
    for (int i = 0; i < kPresets.size(); ++i) {
        double d = qAbs(kPresets[i] - speed);
        if (d < bestDiff) { bestDiff = d; bestIdx = i; }
    }
    if (bestDiff < 0.01) m_preset->setCurrentIndex(bestIdx);

    updateLabel();
    emit speedChanged(m_speed);
}

void SpeedControl::onSliderMoved(int value)
{
    double s = sliderToSpeed(value);
    m_speed  = s;
    updateLabel();
    emit speedChanged(m_speed);
}

void SpeedControl::onPresetSelected(int index)
{
    if (index < 0 || index >= kPresets.size()) return;
    setSpeed(kPresets[index]);
}

void SpeedControl::onReset()
{
    setSpeed(1.0);
}

void SpeedControl::updateLabel()
{
    m_label->setText(QString("%1×").arg(m_speed, 0, 'f', 2));
}

int SpeedControl::speedToSlider(double speed) const
{
    // Linear mapping: 0.05 → 5, 4.0 → 200
    return qRound(speed * 50.0);
}

double SpeedControl::sliderToSpeed(int val) const
{
    return qBound(0.05, val / 50.0, 4.0);
}

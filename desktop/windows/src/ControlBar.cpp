#include "ControlBar.h"
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTime>

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(70);
    setStyleSheet(R"(
        QWidget { background: #0D0D12; }
        QPushButton, QToolButton {
            background: transparent; color: #DDD;
            border: none; font-size: 18px; padding: 4px 10px;
        }
        QPushButton:hover, QToolButton:hover { color: #007ACC; }
        QSlider::groove:horizontal {
            height: 4px; background: #333; border-radius: 2px;
        }
        QSlider::sub-page:horizontal { background: #007ACC; border-radius: 2px; }
        QSlider::handle:horizontal {
            width: 12px; height: 12px; border-radius: 6px;
            background: #FFF; margin: -4px 0;
        }
    )");

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(8, 4, 8, 4);
    vlay->setSpacing(4);

    m_seek = new QSlider(Qt::Horizontal);
    m_seek->setRange(0, 1000);
    vlay->addWidget(m_seek);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);

    m_play = new QPushButton("⏵");
    m_stop = new QToolButton; m_stop->setText("⏹");
    m_fs   = new QToolButton; m_fs->setText("⛶");

    m_vol = new QSlider(Qt::Horizontal);
    m_vol->setRange(0, 100);
    m_vol->setValue(100);
    m_vol->setFixedWidth(90);

    m_timeLbl = new QLabel("0:00 / 0:00");
    m_timeLbl->setStyleSheet("color:#999; font-size:12px;");

    btnRow->addWidget(m_play);
    btnRow->addWidget(m_stop);
    btnRow->addSpacing(8);
    btnRow->addWidget(new QLabel("🔊"));
    btnRow->addWidget(m_vol);
    btnRow->addStretch();
    btnRow->addWidget(m_timeLbl);
    btnRow->addSpacing(8);
    btnRow->addWidget(m_fs);

    vlay->addLayout(btnRow);

    connect(m_play, &QPushButton::clicked,   this, [this] {
        m_play->setText(m_play->text() == "⏵" ? "⏸" : "⏵");
        emit playPauseClicked();
    });
    connect(m_stop, &QToolButton::clicked,   this, &ControlBar::stopClicked);
    connect(m_fs,   &QToolButton::clicked,   this, &ControlBar::fullscreenClicked);
    connect(m_seek, &QSlider::valueChanged,  this, &ControlBar::seeked);
    connect(m_vol,  &QSlider::valueChanged,  this, &ControlBar::volumeChanged);
}

void ControlBar::setPosition(int ms) {
    m_seek->blockSignals(true);
    if (m_duration > 0)
        m_seek->setValue(static_cast<int>(double(ms) / m_duration * 1000));
    m_seek->blockSignals(false);
    m_timeLbl->setText(formatTime(ms) + " / " + formatTime(m_duration));
}

void ControlBar::setDuration(int ms) {
    m_duration = ms;
    m_timeLbl->setText("0:00 / " + formatTime(ms));
}

QString ControlBar::formatTime(int ms) {
    int s = ms / 1000;
    return QTime(0, s/60, s%60).toString(s >= 3600 ? "H:mm:ss" : "m:ss");
}

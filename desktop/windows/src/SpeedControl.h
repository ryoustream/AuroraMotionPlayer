#pragma once
#include <QWidget>

class QSlider;
class QLabel;
class QPushButton;
class QComboBox;

// Compact playback speed selector with slider + preset buttons
class SpeedControl : public QWidget {
    Q_OBJECT
public:
    explicit SpeedControl(QWidget* parent = nullptr);

    double currentSpeed() const { return m_speed; }
    void   setSpeed(double speed);

signals:
    void speedChanged(double speed);

private slots:
    void onSliderMoved(int value);
    void onPresetSelected(int index);
    void onReset();

private:
    void        updateLabel();
    int         speedToSlider(double speed) const;
    double      sliderToSpeed(int val) const;

    double        m_speed   = 1.0;
    QSlider*      m_slider  = nullptr;
    QLabel*       m_label   = nullptr;
    QComboBox*    m_preset  = nullptr;
    QPushButton*  m_resetBtn = nullptr;

    static const QVector<double> kPresets;
};

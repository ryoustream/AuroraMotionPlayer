#pragma once
#include <QWidget>
class QSlider;
class QLabel;
class QPushButton;
class QToolButton;
class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget* parent = nullptr);
    void setPosition(int ms);
    void setDuration(int ms);
signals:
    void playPauseClicked();
    void stopClicked();
    void seeked(int ms);
    void volumeChanged(int percent);
    void fullscreenClicked();
private:
    QSlider*      m_seek    = nullptr;
    QSlider*      m_vol     = nullptr;
    QLabel*       m_timeLbl = nullptr;
    QPushButton*  m_play    = nullptr;
    QToolButton*  m_stop    = nullptr;
    QToolButton*  m_fs      = nullptr;
    int           m_duration= 0;
    static QString formatTime(int ms);
};

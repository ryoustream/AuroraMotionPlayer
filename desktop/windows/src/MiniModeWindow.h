#pragma once
#include <QWidget>
#include <QPoint>

class PlayerWidget;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

// Compact always-on-top floating player window (mini mode).
// Mirrors playback state from the main window; communicates
// via signals/slots — no direct coupling to MainWindow.
class MiniModeWindow : public QWidget {
    Q_OBJECT
public:
    explicit MiniModeWindow(QWidget* parent = nullptr);
    ~MiniModeWindow() override;

    void setTitle(const QString& title);
    void setPosition(double seconds, double duration);
    void setPlaying(bool playing);
    void setVolume(int vol); // 0–100

signals:
    void playPauseClicked();
    void stopClicked();
    void seekRequested(double seconds);
    void volumeChanged(int vol);
    void restoreRequested();   // User wants full window back
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent*  e) override;
    void mouseMoveEvent(QMouseEvent*   e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent*      e) override;
    void paintEvent(QPaintEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onHideControls();

private:
    void setupUI();
    void showControls(bool on);

    // Drag-move state
    QPoint m_dragStart;
    bool   m_dragging   = false;
    bool   m_ctrlVisible = true;

    QString m_title;
    double  m_pos       = 0.0;
    double  m_duration  = 0.0;
    bool    m_playing   = false;

    QWidget*     m_overlay   = nullptr;
    QLabel*      m_titleLbl  = nullptr;
    QPushButton* m_playBtn   = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QPushButton* m_restoreBtn = nullptr;
    QPushButton* m_closeBtn  = nullptr;
    QSlider*     m_seek      = nullptr;
    QSlider*     m_vol       = nullptr;
    QLabel*      m_timeLbl   = nullptr;
    QTimer*      m_hideTimer = nullptr;
};

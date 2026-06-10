#pragma once
#include <QMainWindow>
#include <QStringList>
#include <memory>

class PlayerWidget;
class PlaylistWidget;
class ControlBar;
class SettingsDialog;
class BenchmarkOverlay;
class SubtitleOverlay;
class ThumbnailBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFile(const QString& path);
    void openURL(const QString& url);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onOpen();
    void onOpenURL();
    void onPlayPause();
    void onStop();
    void onSeek(int position);
    void onVolumeChanged(int volume);
    void onFullscreen();
    void onSettings();
    void onShowBenchmark(bool show);
    void onPlaylistItemActivated(const QString& path);
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onFileAssociation();
    void onAbout();
    void onPictureInPicture();
    void onABRepeat();
    void onBookmark();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void applyDarkTheme();
    void saveState();
    void restoreState();
    void updateTitle(const QString& file);
    void showControls(bool visible);
    void startHideControlsTimer();

    std::unique_ptr<PlayerWidget>   m_player;
    std::unique_ptr<PlaylistWidget> m_playlist;
    std::unique_ptr<ControlBar>     m_controlBar;
    std::unique_ptr<SettingsDialog> m_settings;
    std::unique_ptr<BenchmarkOverlay> m_benchmark;
    std::unique_ptr<SubtitleOverlay>  m_subtitle;
    std::unique_ptr<ThumbnailBar>     m_thumbBar;

    QWidget* m_centralWidget = nullptr;
    bool     m_fullscreen    = false;
    bool     m_controlsVisible = true;
    QTimer*  m_hideControlsTimer = nullptr;

    // AB Repeat
    double m_abStart = -1.0, m_abEnd = -1.0;
};

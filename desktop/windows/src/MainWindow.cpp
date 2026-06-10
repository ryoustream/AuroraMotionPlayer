#include "MainWindow.h"
#include "PlayerWidget.h"
#include "PlaylistWidget.h"
#include "ControlBar.h"
#include "SettingsDialog.h"
#include "BenchmarkOverlay.h"
#include "SubtitleOverlay.h"
#include "ThumbnailBar.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QSettings>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Aurora Motion Player");
    setMinimumSize(800, 500);
    resize(1280, 720);
    setAcceptDrops(true);

    setupUI();
    setupMenuBar();
    setupStatusBar();
    applyDarkTheme();
    restoreState();

    m_hideControlsTimer = new QTimer(this);
    m_hideControlsTimer->setSingleShot(true);
    connect(m_hideControlsTimer, &QTimer::timeout, this, [this] {
        if (m_fullscreen) showControls(false);
    });

    installEventFilter(this);
}

MainWindow::~MainWindow() {
    saveState();
}

void MainWindow::setupUI() {
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    auto* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Main content: player + playlist (splitter)
    auto* splitter = new QSplitter(Qt::Horizontal, m_centralWidget);

    // Player area (stack: video + overlays)
    auto* playerArea = new QWidget(splitter);
    auto* playerLayout = new QVBoxLayout(playerArea);
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->setSpacing(0);

    m_player    = std::make_unique<PlayerWidget>(playerArea);
    m_benchmark = std::make_unique<BenchmarkOverlay>(m_player.get());
    m_subtitle  = std::make_unique<SubtitleOverlay>(m_player.get());
    m_thumbBar  = std::make_unique<ThumbnailBar>(playerArea);

    playerLayout->addWidget(m_player.get(), 1);
    playerLayout->addWidget(m_thumbBar.get());

    m_playlist = std::make_unique<PlaylistWidget>(splitter);
    splitter->addWidget(playerArea);
    splitter->addWidget(m_playlist.get());
    splitter->setSizes({1000, 280});
    splitter->setChildrenCollapsible(false);

    m_controlBar = std::make_unique<ControlBar>(m_centralWidget);

    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(m_controlBar.get());

    // Connections
    connect(m_controlBar.get(), &ControlBar::playPauseClicked,  this, &MainWindow::onPlayPause);
    connect(m_controlBar.get(), &ControlBar::stopClicked,       this, &MainWindow::onStop);
    connect(m_controlBar.get(), &ControlBar::seeked,            this, &MainWindow::onSeek);
    connect(m_controlBar.get(), &ControlBar::volumeChanged,     this, &MainWindow::onVolumeChanged);
    connect(m_controlBar.get(), &ControlBar::fullscreenClicked, this, &MainWindow::onFullscreen);
    connect(m_playlist.get(),  &PlaylistWidget::itemActivated,  this, &MainWindow::onPlaylistItemActivated);
    connect(m_player.get(),    &PlayerWidget::positionChanged,  this, &MainWindow::onPositionChanged);
    connect(m_player.get(),    &PlayerWidget::durationChanged,  this, &MainWindow::onDurationChanged);
}

void MainWindow::setupMenuBar() {
    auto* mb = menuBar();

    // File
    auto* fileMenu = mb->addMenu(tr("&File"));
    fileMenu->addAction(tr("Open File..."),       this, &MainWindow::onOpen,           QKeySequence::Open);
    fileMenu->addAction(tr("Open URL..."),         this, &MainWindow::onOpenURL,         tr("Ctrl+U"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("File Association"),    this, &MainWindow::onFileAssociation);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Exit"),                qApp, &QApplication::quit,           QKeySequence::Quit);

    // Playback
    auto* playMenu = mb->addMenu(tr("&Playback"));
    playMenu->addAction(tr("Play/Pause"),   this, &MainWindow::onPlayPause, tr("Space"));
    playMenu->addAction(tr("Stop"),         this, &MainWindow::onStop,      tr("S"));
    playMenu->addSeparator();
    playMenu->addAction(tr("Picture in Picture"), this, &MainWindow::onPictureInPicture, tr("Ctrl+P"));
    playMenu->addAction(tr("A-B Repeat"),          this, &MainWindow::onABRepeat,          tr("R"));
    playMenu->addAction(tr("Bookmark"),            this, &MainWindow::onBookmark,           tr("B"));

    // Video
    auto* videoMenu = mb->addMenu(tr("&Video"));
    videoMenu->addAction(tr("Fullscreen"),     this, &MainWindow::onFullscreen, tr("F"));
    auto* benchAction = videoMenu->addAction(tr("Show Benchmark"), this, &MainWindow::onShowBenchmark);
    benchAction->setCheckable(true);

    // Tools
    auto* toolsMenu = mb->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("Settings..."), this, &MainWindow::onSettings, tr("Ctrl+,"));

    // Help
    auto* helpMenu = mb->addMenu(tr("&Help"));
    helpMenu->addAction(tr("About Aurora Player"), this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar() {
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::applyDarkTheme() {
    qApp->setStyle("Fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(20, 20, 24));
    dark.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Base,            QColor(13, 13, 16));
    dark.setColor(QPalette::AlternateBase,   QColor(30, 30, 36));
    dark.setColor(QPalette::ToolTipBase,     QColor(40, 40, 48));
    dark.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
    dark.setColor(QPalette::Text,            QColor(210, 210, 210));
    dark.setColor(QPalette::Button,          QColor(35, 35, 42));
    dark.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    dark.setColor(QPalette::BrightText,      Qt::white);
    dark.setColor(QPalette::Highlight,       QColor(0, 140, 255));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(dark);
    qApp->setStyleSheet(R"(
        QMenuBar { background: #14141A; color: #DDD; }
        QMenuBar::item:selected { background: #007ACC; }
        QMenu { background: #1E1E28; color: #DDD; border: 1px solid #333; }
        QMenu::item:selected { background: #007ACC; }
        QStatusBar { background: #14141A; color: #999; }
        QSplitter::handle { background: #2A2A35; width: 2px; }
        QScrollBar:vertical { background: #1E1E28; width: 8px; }
        QScrollBar::handle:vertical { background: #444; border-radius: 4px; }
    )");
}

// ── File open / URL ───────────────────────────────────────────────────────────
void MainWindow::openFile(const QString& path) {
    m_player->open(path);
    updateTitle(QFileInfo(path).fileName());
    m_playlist->addItem(path);
    statusBar()->showMessage(tr("Playing: %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::openURL(const QString& url) {
    m_player->open(url);
    updateTitle(url);
    statusBar()->showMessage(tr("Streaming: %1").arg(url));
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, tr("Open Video File"), "",
        tr("Video Files (*.mkv *.mp4 *.avi *.mov *.webm *.m2ts *.ts *.m4v *.flv *.wmv *.3gp);;"
           "All Files (*)"));
    if (!path.isEmpty()) openFile(path);
}

void MainWindow::onOpenURL() {
    bool ok;
    QString url = QInputDialog::getText(this, tr("Open URL"),
        tr("Enter stream URL (HTTP, HLS, RTMP, RTSP...):"),
        QLineEdit::Normal, "", &ok);
    if (ok && !url.isEmpty()) openURL(url);
}

// ── Playback controls ─────────────────────────────────────────────────────────
void MainWindow::onPlayPause()     { m_player->togglePlayPause(); }
void MainWindow::onStop()          { m_player->stop(); updateTitle(""); }
void MainWindow::onSeek(int pos)   { m_player->seek(pos / 1000.0); }
void MainWindow::onVolumeChanged(int v) { m_player->setVolume(v); }

void MainWindow::onPositionChanged(double s)  { m_controlBar->setPosition(static_cast<int>(s * 1000)); }
void MainWindow::onDurationChanged(double s)  { m_controlBar->setDuration(static_cast<int>(s * 1000)); }

// ── Fullscreen ────────────────────────────────────────────────────────────────
void MainWindow::onFullscreen() {
    m_fullscreen = !m_fullscreen;
    if (m_fullscreen) {
        menuBar()->hide();
        statusBar()->hide();
        showFullScreen();
        startHideControlsTimer();
    } else {
        menuBar()->show();
        statusBar()->show();
        showNormal();
        showControls(true);
    }
}

void MainWindow::showControls(bool visible) {
    m_controlsVisible = visible;
    m_controlBar->setVisible(visible);
}

void MainWindow::startHideControlsTimer() {
    m_hideControlsTimer->start(3000);
    showControls(true);
}

// ── Drag & Drop ───────────────────────────────────────────────────────────────
void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    for (const auto& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            openFile(url.toLocalFile());
            break;
        } else {
            openURL(url.toString());
            break;
        }
    }
}

// ── Keyboard ──────────────────────────────────────────────────────────────────
void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Space:  onPlayPause();   break;
    case Qt::Key_F:      onFullscreen();  break;
    case Qt::Key_Escape: if (m_fullscreen) onFullscreen(); break;
    case Qt::Key_S:      onStop();        break;
    case Qt::Key_Left:   m_player->seekRelative(-5.0);  break;
    case Qt::Key_Right:  m_player->seekRelative( 5.0);  break;
    case Qt::Key_Up:     m_player->setVolume(m_player->volume() + 5); break;
    case Qt::Key_Down:   m_player->setVolume(m_player->volume() - 5); break;
    default: QMainWindow::keyPressEvent(e); break;
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* e) {
    onFullscreen();
    QMainWindow::mouseDoubleClickEvent(e);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (m_fullscreen && event->type() == QEvent::MouseMove) {
        startHideControlsTimer();
    }
    return QMainWindow::eventFilter(obj, event);
}

// ── Misc ──────────────────────────────────────────────────────────────────────
void MainWindow::onSettings()          { m_settings->exec(); }
void MainWindow::onShowBenchmark(bool show) { m_benchmark->setVisible(show); }
void MainWindow::onPlaylistItemActivated(const QString& p) { openFile(p); }
void MainWindow::onFileAssociation()   { /* Register file types in Windows registry */ }
void MainWindow::onPictureInPicture()  { /* Spawn PiP window */ }
void MainWindow::onABRepeat()          { /* Toggle A/B points */ }
void MainWindow::onBookmark()          { /* Add bookmark at current position */ }

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("About Aurora Motion Player"),
        tr("<h2>Aurora Motion Player</h2>"
           "<p>Version 1.0.0</p>"
           "<p>AI-powered video player with real-time frame interpolation.</p>"
           "<p>Features: RIFE/FILM interpolation, RealESRGAN upscaling, "
           "HDR10/HLG tone mapping, hardware decoding, and more.</p>"
           "<p>Open source — all features free.</p>"));
}

void MainWindow::updateTitle(const QString& file) {
    setWindowTitle(file.isEmpty() ? "Aurora Motion Player"
                                  : QString("%1 — Aurora Motion Player").arg(file));
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    // Update overlay positions
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveState();
    QMainWindow::closeEvent(e);
}

void MainWindow::saveState() {
    QSettings s("Aurora", "AuroraPlayer");
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", QMainWindow::saveState());
}

void MainWindow::restoreState() {
    QSettings s("Aurora", "AuroraPlayer");
    if (s.contains("geometry"))    restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState")) QMainWindow::restoreState(s.value("windowState").toByteArray());
}

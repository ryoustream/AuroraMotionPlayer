#include "MiniModeWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QCloseEvent>
#include <QScreen>
#include <QGuiApplication>

static const char* kStyle = R"(
QWidget#overlay {
    background: rgba(8,8,16,210);
    border-radius: 8px;
}
QLabel { color: #C0C0E0; font-size: 10px; background: transparent; }
QPushButton {
    background: transparent;
    border: none; color: #9090CC;
    font-size: 14px; padding: 2px;
}
QPushButton:hover { color: #FFFFFF; }
QSlider::groove:horizontal {
    background: #1E1E32; border-radius: 2px; height: 3px;
}
QSlider::handle:horizontal {
    background: #7070CC; border-radius: 4px;
    width: 8px; height: 8px; margin: -3px 0;
}
QSlider::sub-page:horizontal { background: #5050AA; border-radius: 2px; }
)";

MiniModeWindow::MiniModeWindow(QWidget* parent)
    : QWidget(parent,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFixedSize(320, 180);

    // Position: bottom-right of primary screen
    QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    move(screen.right() - 340, screen.bottom() - 200);

    setupUI();

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(2500);
    connect(m_hideTimer, &QTimer::timeout, this, [this]{ showControls(false); });
}

MiniModeWindow::~MiniModeWindow() = default;

void MiniModeWindow::setupUI()
{
    // The semi-transparent overlay covers the whole widget
    m_overlay = new QWidget(this);
    m_overlay->setObjectName("overlay");
    m_overlay->setGeometry(rect());
    m_overlay->setStyleSheet(kStyle);

    auto* vl = new QVBoxLayout(m_overlay);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(3);

    // Title
    m_titleLbl = new QLabel("Aurora Motion Player", m_overlay);
    m_titleLbl->setAlignment(Qt::AlignCenter);
    m_titleLbl->setStyleSheet("font-size:10px; color:#8888AA;");
    vl->addWidget(m_titleLbl);

    // Seek
    m_seek = new QSlider(Qt::Horizontal, m_overlay);
    m_seek->setRange(0, 1000);
    vl->addWidget(m_seek);

    // Buttons row
    auto* hl = new QHBoxLayout;
    hl->setSpacing(2);
    m_stopBtn    = new QPushButton("⏹", m_overlay);
    m_playBtn    = new QPushButton("⏸", m_overlay);
    m_timeLbl    = new QLabel("0:00", m_overlay);
    m_timeLbl->setAlignment(Qt::AlignCenter);
    m_restoreBtn = new QPushButton("⬆", m_overlay);
    m_closeBtn   = new QPushButton("✕", m_overlay);
    m_vol        = new QSlider(Qt::Horizontal, m_overlay);
    m_vol->setRange(0, 100);
    m_vol->setValue(100);
    m_vol->setFixedWidth(60);

    hl->addWidget(m_stopBtn);
    hl->addWidget(m_playBtn);
    hl->addStretch();
    hl->addWidget(m_timeLbl);
    hl->addStretch();
    hl->addWidget(m_vol);
    hl->addWidget(m_restoreBtn);
    hl->addWidget(m_closeBtn);
    vl->addLayout(hl);

    connect(m_playBtn,    &QPushButton::clicked, this, &MiniModeWindow::playPauseClicked);
    connect(m_stopBtn,    &QPushButton::clicked, this, &MiniModeWindow::stopClicked);
    connect(m_restoreBtn, &QPushButton::clicked, this, &MiniModeWindow::restoreRequested);
    connect(m_closeBtn,   &QPushButton::clicked, this, &MiniModeWindow::closeRequested);

    connect(m_seek, &QSlider::valueChanged, this, [this](int v) {
        if (m_duration > 0.0)
            emit seekRequested((v / 1000.0) * m_duration);
    });
    connect(m_vol, &QSlider::valueChanged, this, &MiniModeWindow::volumeChanged);
}

// ── Public setters ─────────────────────────────────────────────────────────

void MiniModeWindow::setTitle(const QString& title)
{
    m_title = title;
    if (m_titleLbl)
        m_titleLbl->setText(title.left(45));
}

void MiniModeWindow::setPosition(double seconds, double duration)
{
    m_pos      = seconds;
    m_duration = duration;

    QSignalBlocker sb(m_seek);
    if (duration > 0.0)
        m_seek->setValue(static_cast<int>((seconds / duration) * 1000.0));

    // Update time label
    int s = static_cast<int>(seconds);
    int mm = s / 60, ss = s % 60;
    if (m_timeLbl)
        m_timeLbl->setText(QString("%1:%2").arg(mm).arg(ss, 2, 10, QChar('0')));
}

void MiniModeWindow::setPlaying(bool playing)
{
    m_playing = playing;
    if (m_playBtn)
        m_playBtn->setText(playing ? "⏸" : "▶");
}

void MiniModeWindow::setVolume(int vol)
{
    QSignalBlocker sb(m_vol);
    m_vol->setValue(vol);
}

// ── UI helpers ─────────────────────────────────────────────────────────────

void MiniModeWindow::showControls(bool on)
{
    m_ctrlVisible = on;
    m_overlay->setVisible(on);
}

void MiniModeWindow::onHideControls()
{
    if (!underMouse()) showControls(false);
}

// ── Events ─────────────────────────────────────────────────────────────────

void MiniModeWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Rounded window background (drawn behind the overlay)
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    p.fillPath(path, QColor(6, 6, 14, 230));

    // Thin border
    p.setPen(QPen(QColor(60, 60, 100, 180), 1.0));
    p.drawPath(path);
}

void MiniModeWindow::enterEvent(QEnterEvent* e)
{
    QWidget::enterEvent(e);
    showControls(true);
    m_hideTimer->stop();
}

void MiniModeWindow::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    m_hideTimer->start();
}

void MiniModeWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStart = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging  = true;
    }
    QWidget::mousePressEvent(e);
}

void MiniModeWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragStart);
    QWidget::mouseMoveEvent(e);
}

void MiniModeWindow::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(e);
}

void MiniModeWindow::mouseDoubleClickEvent(QMouseEvent*)
{
    emit restoreRequested();
}

void MiniModeWindow::closeEvent(QCloseEvent* e)
{
    e->ignore();
    emit closeRequested();
}

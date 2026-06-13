#pragma once
#include <QWidget>
#include <QVector>
#include <QPixmap>
#include <QTimer>

// Displays a row of video preview thumbnails above the seek bar.
// Thumbnails are generated asynchronously via QFuture and cached in memory.
class ThumbnailBar : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailBar(QWidget* parent = nullptr);
    ~ThumbnailBar() override;

    // Called when a new file is opened. Clears old thumbnails.
    void setDuration(double durationSeconds);

    // Update hover position (0.0–1.0) to show preview tooltip
    void setHoverPosition(double fraction);

    // Feed a thumbnail; index is the segment index 0..N-1
    void setThumbnail(int index, const QPixmap& thumb);

    // Number of thumbnail slots
    int slotCount() const { return m_slots; }

    // Expose which time range slot i represents
    double slotTime(int index) const;

    void clear();

signals:
    // Emitted when user clicks on the bar
    void seekRequested(double seconds);
    // Emitted to request generation of thumbnail at seconds
    void thumbnailRequested(int index, double seconds);

protected:
    void paintEvent(QPaintEvent* event)    override;
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void leaveEvent(QEvent* e)             override;
    void resizeEvent(QResizeEvent* e)      override;

private:
    void rebuildSlots();

    double          m_duration    = 0.0;
    int             m_slots       = 0;
    double          m_hoverFrac   = -1.0;
    QVector<QPixmap> m_thumbs;

    static constexpr int   kThumbH   = 68;   // px height
    static constexpr double kMaxSlots = 24.0; // max thumbnails
};

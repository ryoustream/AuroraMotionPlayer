#pragma once
#include <QWidget>
#include <QVector>
#include <QString>

struct Chapter {
    QString title;
    double  startTime = 0.0;   // seconds
    double  endTime   = 0.0;   // seconds
};

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class ChapterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChapterWidget(QWidget* parent = nullptr);

    void setChapters(const QVector<Chapter>& chapters, double totalDuration);
    void setCurrentPosition(double seconds);
    void clear();

    int                      chapterIndexAt(double seconds) const;
    const QVector<Chapter>&  chapters() const { return m_chapters; }

signals:
    void chapterSelected(double startTime);

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onPrev();
    void onNext();

private:
    void    rebuild();
    QString formatTime(double secs) const;
    void    highlightCurrentChapter();

    QVector<Chapter> m_chapters;
    double           m_duration = 0.0;
    double           m_position = 0.0;

    QLabel*      m_titleLabel = nullptr;
    QListWidget* m_list       = nullptr;
    QPushButton* m_prevBtn    = nullptr;
    QPushButton* m_nextBtn    = nullptr;
};

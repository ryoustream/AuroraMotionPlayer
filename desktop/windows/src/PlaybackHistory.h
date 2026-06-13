#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

struct HistoryEntry {
    QString   filePath;
    QString   title;          // display name
    double    lastPosition;   // seconds (resume point)
    double    duration;       // total duration
    QDateTime lastPlayed;
    int       playCount = 0;
    bool      completed = false;  // >90% watched
};

class PlaybackHistory : public QObject {
    Q_OBJECT
public:
    explicit PlaybackHistory(QObject* parent = nullptr);

    void  record(const QString& filePath, const QString& title,
                 double position, double duration);
    void  markCompleted(const QString& filePath);
    void  remove(const QString& filePath);
    void  clearAll();

    HistoryEntry          entryFor(const QString& filePath) const;
    QVector<HistoryEntry> recentEntries(int limit = 50) const;
    bool                  hasEntry(const QString& filePath) const;
    double                resumePosition(const QString& filePath) const;

    bool load(const QString& path = QString());
    bool save(const QString& path = QString()) const;

signals:
    void historyUpdated();

private:
    QString defaultStoragePath() const;
    int     indexFor(const QString& filePath) const;

    QVector<HistoryEntry> m_entries;
    QString               m_storagePath;
    static constexpr int  kMaxEntries = 500;
};

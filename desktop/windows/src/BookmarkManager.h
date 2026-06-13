#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

struct Bookmark {
    QString   id;          // UUID
    QString   filePath;
    double    position;    // seconds
    QString   label;
    QDateTime createdAt;
    QString   thumbnailPath; // optional cached thumbnail
};

class BookmarkManager : public QObject {
    Q_OBJECT
public:
    explicit BookmarkManager(QObject* parent = nullptr);

    // CRUD
    Bookmark  addBookmark(const QString& filePath, double position,
                          const QString& label = QString());
    bool      removeBookmark(const QString& id);
    bool      updateLabel(const QString& id, const QString& newLabel);
    void      clearAll();

    // Query
    QVector<Bookmark> bookmarksForFile(const QString& filePath) const;
    QVector<Bookmark> allBookmarks() const;
    Bookmark          bookmarkById(const QString& id) const;

    // Persistence
    bool load(const QString& path = QString());
    bool save(const QString& path = QString()) const;

signals:
    void bookmarkAdded(const Bookmark& bm);
    void bookmarkRemoved(const QString& id);
    void bookmarkUpdated(const Bookmark& bm);

private:
    QString defaultStoragePath() const;

    QVector<Bookmark> m_bookmarks;
    QString           m_storagePath;
};

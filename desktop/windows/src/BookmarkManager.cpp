#include "BookmarkManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

BookmarkManager::BookmarkManager(QObject* parent) : QObject(parent)
{
    m_storagePath = defaultStoragePath();
    load();
}

Bookmark BookmarkManager::addBookmark(const QString& filePath,
                                       double        position,
                                       const QString& label)
{
    Bookmark bm;
    bm.id          = QUuid::createUuid().toString(QUuid::WithoutBraces);
    bm.filePath    = filePath;
    bm.position    = position;
    bm.label       = label.isEmpty()
                       ? QString("Bookmark at %1").arg(
                             QDateTime::currentDateTime().toString("hh:mm:ss"))
                       : label;
    bm.createdAt   = QDateTime::currentDateTime();
    m_bookmarks.append(bm);
    save();
    emit bookmarkAdded(bm);
    return bm;
}

bool BookmarkManager::removeBookmark(const QString& id)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == id) {
            m_bookmarks.removeAt(i);
            save();
            emit bookmarkRemoved(id);
            return true;
        }
    }
    return false;
}

bool BookmarkManager::updateLabel(const QString& id, const QString& newLabel)
{
    for (auto& bm : m_bookmarks) {
        if (bm.id == id) {
            bm.label = newLabel;
            save();
            emit bookmarkUpdated(bm);
            return true;
        }
    }
    return false;
}

void BookmarkManager::clearAll()
{
    m_bookmarks.clear();
    save();
}

QVector<Bookmark> BookmarkManager::bookmarksForFile(const QString& filePath) const
{
    QVector<Bookmark> result;
    for (const auto& bm : m_bookmarks)
        if (bm.filePath == filePath)
            result.append(bm);
    return result;
}

QVector<Bookmark> BookmarkManager::allBookmarks() const { return m_bookmarks; }

Bookmark BookmarkManager::bookmarkById(const QString& id) const
{
    for (const auto& bm : m_bookmarks)
        if (bm.id == id) return bm;
    return {};
}

bool BookmarkManager::load(const QString& path)
{
    QString p = path.isEmpty() ? m_storagePath : path;
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return false;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return false;

    m_bookmarks.clear();
    for (const auto& v : doc.array()) {
        auto o = v.toObject();
        Bookmark bm;
        bm.id             = o["id"].toString();
        bm.filePath       = o["filePath"].toString();
        bm.position       = o["position"].toDouble();
        bm.label          = o["label"].toString();
        bm.createdAt      = QDateTime::fromString(o["createdAt"].toString(), Qt::ISODate);
        bm.thumbnailPath  = o["thumbnailPath"].toString();
        if (!bm.id.isEmpty())
            m_bookmarks.append(bm);
    }
    return true;
}

bool BookmarkManager::save(const QString& path) const
{
    QString p = path.isEmpty() ? m_storagePath : path;
    QDir().mkpath(QFileInfo(p).absolutePath());

    QJsonArray arr;
    for (const auto& bm : m_bookmarks) {
        QJsonObject o;
        o["id"]            = bm.id;
        o["filePath"]      = bm.filePath;
        o["position"]      = bm.position;
        o["label"]         = bm.label;
        o["createdAt"]     = bm.createdAt.toString(Qt::ISODate);
        o["thumbnailPath"] = bm.thumbnailPath;
        arr.append(o);
    }

    QFile f(p);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return true;
}

QString BookmarkManager::defaultStoragePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/bookmarks.json";
}

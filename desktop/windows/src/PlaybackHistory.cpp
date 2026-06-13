#include "PlaybackHistory.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>

PlaybackHistory::PlaybackHistory(QObject* parent) : QObject(parent)
{
    m_storagePath = defaultStoragePath();
    load();
}

void PlaybackHistory::record(const QString& filePath, const QString& title,
                              double position, double duration)
{
    int idx = indexFor(filePath);
    if (idx >= 0) {
        auto& e = m_entries[idx];
        e.lastPosition = position;
        e.duration     = duration;
        e.lastPlayed   = QDateTime::currentDateTime();
        e.playCount++;
        e.title        = title.isEmpty() ? e.title : title;
        if (duration > 0 && position / duration >= 0.9)
            e.completed = true;
        // Move to front
        HistoryEntry tmp = e;
        m_entries.removeAt(idx);
        m_entries.prepend(tmp);
    } else {
        HistoryEntry e;
        e.filePath     = filePath;
        e.title        = title.isEmpty()
                           ? QFileInfo(filePath).fileName()
                           : title;
        e.lastPosition = position;
        e.duration     = duration;
        e.lastPlayed   = QDateTime::currentDateTime();
        e.playCount    = 1;
        e.completed    = (duration > 0 && position / duration >= 0.9);
        m_entries.prepend(e);
    }

    // Trim to max entries
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();

    save();
    emit historyUpdated();
}

void PlaybackHistory::markCompleted(const QString& filePath)
{
    int idx = indexFor(filePath);
    if (idx >= 0) {
        m_entries[idx].completed = true;
        save();
        emit historyUpdated();
    }
}

void PlaybackHistory::remove(const QString& filePath)
{
    int idx = indexFor(filePath);
    if (idx >= 0) {
        m_entries.removeAt(idx);
        save();
        emit historyUpdated();
    }
}

void PlaybackHistory::clearAll()
{
    m_entries.clear();
    save();
    emit historyUpdated();
}

HistoryEntry PlaybackHistory::entryFor(const QString& filePath) const
{
    int idx = indexFor(filePath);
    return (idx >= 0) ? m_entries[idx] : HistoryEntry{};
}

QVector<HistoryEntry> PlaybackHistory::recentEntries(int limit) const
{
    return m_entries.mid(0, qMin(limit, m_entries.size()));
}

bool PlaybackHistory::hasEntry(const QString& filePath) const
{
    return indexFor(filePath) >= 0;
}

double PlaybackHistory::resumePosition(const QString& filePath) const
{
    int idx = indexFor(filePath);
    if (idx >= 0) {
        const auto& e = m_entries[idx];
        // Don't resume if completed or position < 5s
        if (e.completed || e.lastPosition < 5.0) return 0.0;
        return e.lastPosition;
    }
    return 0.0;
}

bool PlaybackHistory::load(const QString& path)
{
    QString p = path.isEmpty() ? m_storagePath : path;
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) return false;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return false;

    m_entries.clear();
    for (const auto& v : doc.array()) {
        auto o = v.toObject();
        HistoryEntry e;
        e.filePath     = o["filePath"].toString();
        e.title        = o["title"].toString();
        e.lastPosition = o["lastPosition"].toDouble();
        e.duration     = o["duration"].toDouble();
        e.lastPlayed   = QDateTime::fromString(o["lastPlayed"].toString(), Qt::ISODate);
        e.playCount    = o["playCount"].toInt(1);
        e.completed    = o["completed"].toBool(false);
        if (!e.filePath.isEmpty())
            m_entries.append(e);
    }
    return true;
}

bool PlaybackHistory::save(const QString& path) const
{
    QString p = path.isEmpty() ? m_storagePath : path;
    QDir().mkpath(QFileInfo(p).absolutePath());

    QJsonArray arr;
    for (const auto& e : m_entries) {
        QJsonObject o;
        o["filePath"]     = e.filePath;
        o["title"]        = e.title;
        o["lastPosition"] = e.lastPosition;
        o["duration"]     = e.duration;
        o["lastPlayed"]   = e.lastPlayed.toString(Qt::ISODate);
        o["playCount"]    = e.playCount;
        o["completed"]    = e.completed;
        arr.append(o);
    }

    QFile f(p);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return true;
}

QString PlaybackHistory::defaultStoragePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/history.json";
}

int PlaybackHistory::indexFor(const QString& filePath) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].filePath == filePath) return i;
    return -1;
}

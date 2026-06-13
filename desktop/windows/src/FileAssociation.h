#pragma once
#include <QObject>
#include <QStringList>

// Registers/unregisters Aurora as the default handler for video file
// extensions on Windows. Uses shell registry APIs via Windows.h.
class FileAssociation : public QObject {
    Q_OBJECT
public:
    explicit FileAssociation(QObject* parent = nullptr);

    // Register all supported extensions
    bool registerAll(bool makeDefault = true);

    // Unregister (restore previous defaults)
    bool unregisterAll();

    // Register / unregister a single extension (e.g. ".mkv")
    bool registerExt(const QString& ext, bool makeDefault = true);
    bool unregisterExt(const QString& ext);

    // Query
    bool isRegistered(const QString& ext) const;
    bool isDefault(const QString& ext) const;

    // Notify shell about changes
    static void notifyShell();

    static const QStringList kSupportedExtensions;

private:
    bool   writeProgId(const QString& ext) const;
    bool   removeProgId(const QString& ext) const;
    QString progId(const QString& ext) const;
    QString executablePath() const;
};

#include "FileAssociation.h"
#include <QCoreApplication>
#include <QFileInfo>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shlobj.h>
#  pragma comment(lib, "shell32.lib")
#endif

const QStringList FileAssociation::kSupportedExtensions = {
    ".mkv", ".mp4", ".avi", ".mov", ".webm",
    ".m2ts", ".ts",  ".flv", ".wmv", ".m4v",
    ".3gp",  ".ogv", ".vob", ".rmvb"
};

FileAssociation::FileAssociation(QObject* parent) : QObject(parent) {}

bool FileAssociation::registerAll(bool makeDefault)
{
    bool ok = true;
    for (const auto& ext : kSupportedExtensions)
        ok &= registerExt(ext, makeDefault);
    notifyShell();
    return ok;
}

bool FileAssociation::unregisterAll()
{
    bool ok = true;
    for (const auto& ext : kSupportedExtensions)
        ok &= unregisterExt(ext);
    notifyShell();
    return ok;
}

bool FileAssociation::registerExt(const QString& ext, bool makeDefault)
{
#ifdef Q_OS_WIN
    if (!writeProgId(ext)) return false;

    // Associate extension with ProgID
    QString pid = progId(ext);
    HKEY hKey;
    QString keyPath = "Software\\Classes\\" + ext;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        keyPath.toStdWString().c_str(),
                        0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        if (makeDefault) {
            std::wstring ws = pid.toStdWString();
            RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(ws.c_str()),
                           static_cast<DWORD>((ws.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    Q_UNUSED(ext); Q_UNUSED(makeDefault);
    return false; // Linux/macOS not implemented
#endif
}

bool FileAssociation::unregisterExt(const QString& ext)
{
#ifdef Q_OS_WIN
    removeProgId(ext);
    QString keyPath = "Software\\Classes\\" + ext;
    RegDeleteKeyW(HKEY_CURRENT_USER, keyPath.toStdWString().c_str());
    return true;
#else
    Q_UNUSED(ext);
    return false;
#endif
}

bool FileAssociation::isRegistered(const QString& ext) const
{
#ifdef Q_OS_WIN
    QString keyPath = "Software\\Classes\\" + progId(ext);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      keyPath.toStdWString().c_str(),
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    Q_UNUSED(ext);
    return false;
#endif
}

bool FileAssociation::isDefault(const QString& ext) const
{
#ifdef Q_OS_WIN
    QString keyPath = "Software\\Classes\\" + ext;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      keyPath.toStdWString().c_str(),
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    wchar_t buf[256] = {};
    DWORD size = sizeof(buf);
    LONG  res = RegQueryValueExW(hKey, nullptr, nullptr, nullptr,
                                  reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(hKey);
    if (res != ERROR_SUCCESS) return false;
    return QString::fromWCharArray(buf) == progId(ext);
#else
    Q_UNUSED(ext);
    return false;
#endif
}

void FileAssociation::notifyShell()
{
#ifdef Q_OS_WIN
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif
}

bool FileAssociation::writeProgId(const QString& ext) const
{
#ifdef Q_OS_WIN
    QString pid     = progId(ext);
    QString exePath = executablePath();
    QString openCmd = "\"" + exePath + "\" \"%1\"";

    // HKCU\Software\Classes\<ProgID>
    auto writeKey = [&](const QString& subKey, const QString& value) -> bool {
        HKEY hKey;
        QString full = "Software\\Classes\\" + pid + "\\" + subKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER,
                            full.toStdWString().c_str(),
                            0, nullptr, REG_OPTION_NON_VOLATILE,
                            KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
            return false;
        std::wstring ws = value.toStdWString();
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(ws.c_str()),
                       static_cast<DWORD>((ws.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return true;
    };

    writeKey("",                         "Aurora Motion Player Video File");
    writeKey("DefaultIcon",              exePath + ",0");
    writeKey("shell\\open\\command",     openCmd);
    writeKey("shell\\open",              "Open with Aurora Motion Player");
    return true;
#else
    Q_UNUSED(ext);
    return false;
#endif
}

bool FileAssociation::removeProgId(const QString& ext) const
{
#ifdef Q_OS_WIN
    QString pid     = progId(ext);
    QString keyPath = "Software\\Classes\\" + pid;
    // Recursively delete key
    RegDeleteTreeW(HKEY_CURRENT_USER, keyPath.toStdWString().c_str());
    return true;
#else
    Q_UNUSED(ext);
    return false;
#endif
}

QString FileAssociation::progId(const QString& ext) const
{
    // e.g. ".mkv" → "AuroraPlayer.mkv"
    QString e = ext;
    if (e.startsWith('.')) e = e.mid(1);
    return "AuroraPlayer." + e.toLower();
}

QString FileAssociation::executablePath() const
{
    return QCoreApplication::applicationFilePath().replace('/', '\\');
}

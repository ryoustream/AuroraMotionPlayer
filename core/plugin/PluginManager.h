#pragma once
/**
 * Aurora Motion Player — Plugin Manager
 *
 * Discovers, loads, sandboxes, and manages plugin lifecycle.
 * Plugins are shared libraries (.dll / .so) placed in the plugins/ directory.
 *
 * Thread-safety: all public methods are thread-safe.
 */

#include "../../sdk/plugin/AuroraPlugin.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurora::core {

// ── Plugin load result ────────────────────────────────────────────────────────
enum class PluginLoadResult {
    Ok,
    FileNotFound,
    InvalidFormat,
    ApiVersionMismatch,
    InitFailed,
    AlreadyLoaded,
    SandboxError,
};

std::string_view toString(PluginLoadResult r);

// ── Plugin handle ─────────────────────────────────────────────────────────────
struct PluginHandle {
    std::string          id;          // unique: name@version
    sdk::PluginInfo      info;
    sdk::IPlugin*        instance   = nullptr;
    bool                 active     = false;

    // Non-copyable
    PluginHandle()                               = default;
    PluginHandle(const PluginHandle&)            = delete;
    PluginHandle& operator=(const PluginHandle&) = delete;
    PluginHandle(PluginHandle&&)                 = default;
    PluginHandle& operator=(PluginHandle&&)      = default;

    ~PluginHandle();

private:
    friend class PluginManager;
    void*  libHandle   = nullptr;   // dlopen / LoadLibrary handle
    void (*destroyFn)(sdk::IPlugin*) = nullptr;
};

// ── Event callbacks ───────────────────────────────────────────────────────────
using PluginEventCb = std::function<void(const PluginHandle&)>;

// ── Plugin Manager ────────────────────────────────────────────────────────────
class PluginManager {
public:
    static PluginManager& instance();

    // ----- Directory management ----------------------------------------------
    void addSearchPath(const std::filesystem::path& dir);
    void clearSearchPaths();

    // ----- Discovery & loading -----------------------------------------------
    /// Scan all search paths and load all valid plugins.
    int  discoverAndLoad();

    /// Load a specific plugin file.
    PluginLoadResult loadPlugin(const std::filesystem::path& path);

    /// Unload a plugin by id ("name@version").
    bool unloadPlugin(const std::string& id);

    /// Reload a plugin (unload + load).
    PluginLoadResult reloadPlugin(const std::string& id);

    // ----- Query -------------------------------------------------------------
    std::vector<const PluginHandle*> allPlugins() const;
    std::vector<const PluginHandle*> pluginsOfType(sdk::PluginType type) const;
    const PluginHandle*              findPlugin(const std::string& id) const;

    // Typed accessors (returns nullptr if type mismatch)
    sdk::IVideoFilterPlugin*   videoFilter(const std::string& id) const;
    sdk::IAudioFilterPlugin*   audioFilter(const std::string& id) const;
    sdk::INetworkSourcePlugin* networkSource(const std::string& id) const;
    sdk::IAIModelPlugin*       aiModel(const std::string& id) const;

    // ----- Activation --------------------------------------------------------
    bool activatePlugin(const std::string& id);
    bool deactivatePlugin(const std::string& id);

    // ----- Events ------------------------------------------------------------
    void onPluginLoaded(PluginEventCb cb)    { m_onLoaded   = std::move(cb); }
    void onPluginUnloaded(PluginEventCb cb)  { m_onUnloaded = std::move(cb); }

    // ----- Lifecycle ---------------------------------------------------------
    void shutdown();

private:
    PluginManager()  = default;
    ~PluginManager() = default;

    PluginLoadResult loadFromPath(const std::filesystem::path& path,
                                  PluginHandle& out);

    mutable std::mutex                                    m_mutex;
    std::vector<std::filesystem::path>                    m_searchPaths;
    std::unordered_map<std::string, PluginHandle>         m_plugins;
    PluginEventCb                                         m_onLoaded;
    PluginEventCb                                         m_onUnloaded;
};

} // namespace aurora::core

/**
 * Aurora Motion Player — Plugin Manager Implementation
 */

#include "PluginManager.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define LOAD_LIB(p)   LoadLibraryW((p).wstring().c_str())
#  define FIND_SYM(h,s) GetProcAddress(static_cast<HMODULE>(h), (s))
#  define CLOSE_LIB(h)  FreeLibrary(static_cast<HMODULE>(h))
#  define LIB_EXT       L".dll"
#else
#  include <dlfcn.h>
#  define LOAD_LIB(p)   dlopen((p).c_str(), RTLD_NOW | RTLD_LOCAL)
#  define FIND_SYM(h,s) dlsym((h), (s))
#  define CLOSE_LIB(h)  dlclose(h)
#  define LIB_EXT       ".so"
#endif

namespace aurora::core {

// ── toString ─────────────────────────────────────────────────────────────────
std::string_view toString(PluginLoadResult r) {
    switch (r) {
        case PluginLoadResult::Ok:                  return "Ok";
        case PluginLoadResult::FileNotFound:        return "FileNotFound";
        case PluginLoadResult::InvalidFormat:       return "InvalidFormat";
        case PluginLoadResult::ApiVersionMismatch:  return "ApiVersionMismatch";
        case PluginLoadResult::InitFailed:          return "InitFailed";
        case PluginLoadResult::AlreadyLoaded:       return "AlreadyLoaded";
        case PluginLoadResult::SandboxError:        return "SandboxError";
    }
    return "Unknown";
}

// ── PluginHandle destructor ───────────────────────────────────────────────────
PluginHandle::~PluginHandle() {
    if (instance && destroyFn) {
        try {
            destroyFn(instance);
        } catch (...) {
            // Never throw from destructor
        }
        instance = nullptr;
    }
    if (libHandle) {
        CLOSE_LIB(libHandle);
        libHandle = nullptr;
    }
}

// ── Singleton ─────────────────────────────────────────────────────────────────
PluginManager& PluginManager::instance() {
    static PluginManager s_instance;
    return s_instance;
}

// ── Search paths ──────────────────────────────────────────────────────────────
void PluginManager::addSearchPath(const std::filesystem::path& dir) {
    std::lock_guard lock(m_mutex);
    m_searchPaths.push_back(dir);
}

void PluginManager::clearSearchPaths() {
    std::lock_guard lock(m_mutex);
    m_searchPaths.clear();
}

// ── Discover & load ───────────────────────────────────────────────────────────
int PluginManager::discoverAndLoad() {
    int loaded = 0;
    std::vector<std::filesystem::path> paths;
    {
        std::lock_guard lock(m_mutex);
        paths = m_searchPaths;
    }

    for (const auto& dir : paths) {
        if (!std::filesystem::exists(dir)) continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
#ifdef _WIN32
            if (entry.path().extension() != ".dll") continue;
#else
            if (entry.path().extension() != ".so") continue;
#endif
            auto result = loadPlugin(entry.path());
            if (result == PluginLoadResult::Ok) {
                ++loaded;
            } else if (result != PluginLoadResult::AlreadyLoaded) {
                std::cerr << "[PluginManager] Failed to load "
                          << entry.path().filename().string()
                          << ": " << toString(result) << "\n";
            }
        }
    }
    return loaded;
}

// ── Load plugin ───────────────────────────────────────────────────────────────
PluginLoadResult PluginManager::loadPlugin(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return PluginLoadResult::FileNotFound;

    PluginHandle handle;
    auto result = loadFromPath(path, handle);
    if (result != PluginLoadResult::Ok) return result;

    std::lock_guard lock(m_mutex);
    if (m_plugins.count(handle.id)) return PluginLoadResult::AlreadyLoaded;

    auto& ref = m_plugins[handle.id];
    ref = std::move(handle);

    if (m_onLoaded) m_onLoaded(ref);
    return PluginLoadResult::Ok;
}

PluginLoadResult PluginManager::loadFromPath(const std::filesystem::path& path,
                                              PluginHandle& out) {
    // Load shared library
    void* lib = LOAD_LIB(path);
    if (!lib) {
        std::cerr << "[PluginManager] Cannot open " << path << "\n";
        return PluginLoadResult::InvalidFormat;
    }

    // Resolve required exports
    using InfoFn    = sdk::PluginInfo (*)();
    using CreateFn  = sdk::IPlugin* (*)();
    using DestroyFn = void (*)(sdk::IPlugin*);

    auto infoFn    = reinterpret_cast<InfoFn>   (FIND_SYM(lib, "aurora_plugin_info"));
    auto createFn  = reinterpret_cast<CreateFn> (FIND_SYM(lib, "aurora_plugin_create"));
    auto destroyFn = reinterpret_cast<DestroyFn>(FIND_SYM(lib, "aurora_plugin_destroy"));

    if (!infoFn || !createFn || !destroyFn) {
        CLOSE_LIB(lib);
        return PluginLoadResult::InvalidFormat;
    }

    // Check API version
    sdk::PluginInfo info;
    try {
        info = infoFn();
    } catch (...) {
        CLOSE_LIB(lib);
        return PluginLoadResult::InvalidFormat;
    }

    if (info.apiVersion != AURORA_PLUGIN_API_VERSION) {
        CLOSE_LIB(lib);
        return PluginLoadResult::ApiVersionMismatch;
    }

    // Validate required fields
    if (!info.name || !info.version) {
        CLOSE_LIB(lib);
        return PluginLoadResult::InvalidFormat;
    }

    // Instantiate
    sdk::IPlugin* plugin = nullptr;
    try {
        plugin = createFn();
    } catch (...) {
        CLOSE_LIB(lib);
        return PluginLoadResult::SandboxError;
    }

    if (!plugin) {
        CLOSE_LIB(lib);
        return PluginLoadResult::InitFailed;
    }

    // Init
    bool ok = false;
    try {
        ok = plugin->init();
    } catch (...) {
        destroyFn(plugin);
        CLOSE_LIB(lib);
        return PluginLoadResult::SandboxError;
    }

    if (!ok) {
        destroyFn(plugin);
        CLOSE_LIB(lib);
        return PluginLoadResult::InitFailed;
    }

    // Populate handle
    out.id        = std::string(info.name) + "@" + std::string(info.version);
    out.info      = info;
    out.instance  = plugin;
    out.active    = true;
    out.libHandle = lib;
    out.destroyFn = destroyFn;

    std::cout << "[PluginManager] Loaded plugin: " << out.id << "\n";
    return PluginLoadResult::Ok;
}

// ── Unload ────────────────────────────────────────────────────────────────────
bool PluginManager::unloadPlugin(const std::string& id) {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(id);
    if (it == m_plugins.end()) return false;

    if (m_onUnloaded) m_onUnloaded(it->second);
    m_plugins.erase(it);
    return true;
}

PluginLoadResult PluginManager::reloadPlugin(const std::string& id) {
    std::string path;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_plugins.find(id);
        if (it == m_plugins.end()) return PluginLoadResult::FileNotFound;
        // We don't store path currently — future enhancement
        (void)it;
    }
    unloadPlugin(id);
    // Re-discover from search paths
    return discoverAndLoad() > 0 ? PluginLoadResult::Ok : PluginLoadResult::FileNotFound;
}

// ── Query ─────────────────────────────────────────────────────────────────────
std::vector<const PluginHandle*> PluginManager::allPlugins() const {
    std::lock_guard lock(m_mutex);
    std::vector<const PluginHandle*> result;
    result.reserve(m_plugins.size());
    for (const auto& [id, h] : m_plugins) result.push_back(&h);
    return result;
}

std::vector<const PluginHandle*> PluginManager::pluginsOfType(sdk::PluginType type) const {
    std::lock_guard lock(m_mutex);
    std::vector<const PluginHandle*> result;
    for (const auto& [id, h] : m_plugins)
        if (h.info.type == type) result.push_back(&h);
    return result;
}

const PluginHandle* PluginManager::findPlugin(const std::string& id) const {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(id);
    return it != m_plugins.end() ? &it->second : nullptr;
}

sdk::IVideoFilterPlugin* PluginManager::videoFilter(const std::string& id) const {
    auto* h = findPlugin(id);
    if (!h || h->info.type != sdk::PluginType::VideoFilter) return nullptr;
    return dynamic_cast<sdk::IVideoFilterPlugin*>(h->instance);
}

sdk::IAudioFilterPlugin* PluginManager::audioFilter(const std::string& id) const {
    auto* h = findPlugin(id);
    if (!h || h->info.type != sdk::PluginType::AudioFilter) return nullptr;
    return dynamic_cast<sdk::IAudioFilterPlugin*>(h->instance);
}

sdk::INetworkSourcePlugin* PluginManager::networkSource(const std::string& id) const {
    auto* h = findPlugin(id);
    if (!h || h->info.type != sdk::PluginType::NetworkSource) return nullptr;
    return dynamic_cast<sdk::INetworkSourcePlugin*>(h->instance);
}

sdk::IAIModelPlugin* PluginManager::aiModel(const std::string& id) const {
    auto* h = findPlugin(id);
    if (!h || h->info.type != sdk::PluginType::AIModel) return nullptr;
    return dynamic_cast<sdk::IAIModelPlugin*>(h->instance);
}

// ── Activation ────────────────────────────────────────────────────────────────
bool PluginManager::activatePlugin(const std::string& id) {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(id);
    if (it == m_plugins.end()) return false;
    it->second.active = true;
    return true;
}

bool PluginManager::deactivatePlugin(const std::string& id) {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(id);
    if (it == m_plugins.end()) return false;
    it->second.active = false;
    return true;
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void PluginManager::shutdown() {
    std::lock_guard lock(m_mutex);
    m_plugins.clear(); // Destructors call destroy + unload
    std::cout << "[PluginManager] All plugins unloaded.\n";
}

} // namespace aurora::core

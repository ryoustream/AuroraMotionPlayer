/**
 * Aurora Motion Player — Unit Tests: Plugin Manager
 */

#include <gtest/gtest.h>
#include "core/plugin/PluginManager.h"

using namespace aurora::core;
using namespace aurora::sdk;

// ── toString tests ────────────────────────────────────────────────────────────
TEST(PluginLoadResult, ToStringCoversAllCases) {
    EXPECT_EQ(toString(PluginLoadResult::Ok),                 "Ok");
    EXPECT_EQ(toString(PluginLoadResult::FileNotFound),       "FileNotFound");
    EXPECT_EQ(toString(PluginLoadResult::InvalidFormat),      "InvalidFormat");
    EXPECT_EQ(toString(PluginLoadResult::ApiVersionMismatch), "ApiVersionMismatch");
    EXPECT_EQ(toString(PluginLoadResult::InitFailed),         "InitFailed");
    EXPECT_EQ(toString(PluginLoadResult::AlreadyLoaded),      "AlreadyLoaded");
    EXPECT_EQ(toString(PluginLoadResult::SandboxError),       "SandboxError");
}

// ── Singleton ─────────────────────────────────────────────────────────────────
TEST(PluginManager, SingletonReturnsSameInstance) {
    auto& a = PluginManager::instance();
    auto& b = PluginManager::instance();
    EXPECT_EQ(&a, &b);
}

// ── Search paths ──────────────────────────────────────────────────────────────
TEST(PluginManager, AddAndClearSearchPaths) {
    auto& mgr = PluginManager::instance();
    mgr.clearSearchPaths();
    mgr.addSearchPath("/tmp/plugins_a");
    mgr.addSearchPath("/tmp/plugins_b");
    // No crash expected; paths stored internally
    mgr.clearSearchPaths();
}

// ── Load non-existent file ────────────────────────────────────────────────────
TEST(PluginManager, LoadNonExistentFileReturnsFileNotFound) {
    auto& mgr    = PluginManager::instance();
    auto  result = mgr.loadPlugin("/nonexistent/path/fake.dll");
    EXPECT_EQ(result, PluginLoadResult::FileNotFound);
}

// ── Query empty manager ───────────────────────────────────────────────────────
TEST(PluginManager, AllPluginsEmptyOnStart) {
    auto& mgr = PluginManager::instance();
    mgr.shutdown();
    auto plugins = mgr.allPlugins();
    EXPECT_TRUE(plugins.empty());
}

TEST(PluginManager, PluginsOfTypeReturnsEmpty) {
    auto& mgr     = PluginManager::instance();
    auto  vfilters = mgr.pluginsOfType(PluginType::VideoFilter);
    EXPECT_TRUE(vfilters.empty());
}

TEST(PluginManager, FindPluginReturnsNullptrForUnknownId) {
    auto& mgr = PluginManager::instance();
    EXPECT_EQ(mgr.findPlugin("unknown@1.0"), nullptr);
}

// ── Typed accessors return nullptr when not found ─────────────────────────────
TEST(PluginManager, TypedAccessorsReturnNullptr) {
    auto& mgr = PluginManager::instance();
    EXPECT_EQ(mgr.videoFilter("x@1"),   nullptr);
    EXPECT_EQ(mgr.audioFilter("x@1"),   nullptr);
    EXPECT_EQ(mgr.networkSource("x@1"), nullptr);
    EXPECT_EQ(mgr.aiModel("x@1"),       nullptr);
}

// ── Activation on missing plugin ──────────────────────────────────────────────
TEST(PluginManager, ActivateMissingReturnsFalse) {
    auto& mgr = PluginManager::instance();
    EXPECT_FALSE(mgr.activatePlugin("ghost@0.1"));
    EXPECT_FALSE(mgr.deactivatePlugin("ghost@0.1"));
}

// ── Discover on non-existent paths doesn't crash ──────────────────────────────
TEST(PluginManager, DiscoverWithNoValidPathsReturnsZero) {
    auto& mgr = PluginManager::instance();
    mgr.clearSearchPaths();
    mgr.addSearchPath("/tmp/aurora_no_such_plugins_dir_xyz");
    int loaded = mgr.discoverAndLoad();
    EXPECT_EQ(loaded, 0);
    mgr.clearSearchPaths();
}

// ── Shutdown is idempotent ────────────────────────────────────────────────────
TEST(PluginManager, ShutdownIsIdempotent) {
    auto& mgr = PluginManager::instance();
    EXPECT_NO_THROW(mgr.shutdown());
    EXPECT_NO_THROW(mgr.shutdown());
}

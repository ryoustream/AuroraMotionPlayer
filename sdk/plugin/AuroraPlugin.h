#pragma once
/**
 * Aurora Motion Player Plugin SDK
 *
 * Third-party developers can implement any of these interfaces
 * to create plugins for video filters, audio filters, subtitle filters,
 * network sources, and AI models.
 *
 * Plugin discovery: shared libraries (.dll/.so) placed in the
 * plugins/ directory are loaded at startup.
 *
 * Each plugin must export:
 *   aurora_plugin_info()    → PluginInfo
 *   aurora_plugin_create()  → IPlugin*
 *   aurora_plugin_destroy() → void
 */

#include <cstdint>
#include <string>

#define AURORA_PLUGIN_API_VERSION 1

namespace aurora::sdk {

enum class PluginType {
    VideoFilter,
    AudioFilter,
    SubtitleFilter,
    NetworkSource,
    AIModel,
};

struct PluginInfo {
    int         apiVersion   = AURORA_PLUGIN_API_VERSION;
    PluginType  type         = PluginType::VideoFilter;
    const char* name         = nullptr;
    const char* version      = nullptr;
    const char* description  = nullptr;
    const char* author       = nullptr;
    const char* license      = nullptr;
};

// ── Base plugin interface ──────────────────────────────────────────────────────
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual bool        init()     = 0;
    virtual void        shutdown() = 0;
    virtual PluginInfo  info()     const = 0;
};

// ── Video filter plugin ────────────────────────────────────────────────────────
struct VideoFrame {
    uint8_t* data[4]     = {};
    int      linesize[4] = {};
    int      width       = 0;
    int      height      = 0;
    int64_t  pts         = 0;
    int      pixFmt      = 0;  // maps to AVPixelFormat
};

class IVideoFilterPlugin : public IPlugin {
public:
    // Process a video frame in-place or return a new frame
    virtual VideoFrame* process(VideoFrame* frame) = 0;
    virtual void        setParam(const char* key, const char* value) = 0;
    virtual const char* getParam(const char* key) = 0;
};

// ── Audio filter plugin ────────────────────────────────────────────────────────
struct AudioFrame {
    float*   samples   = nullptr; // interleaved float PCM
    int      nbSamples = 0;
    int      channels  = 0;
    int      sampleRate = 0;
    int64_t  pts        = 0;
};

class IAudioFilterPlugin : public IPlugin {
public:
    virtual AudioFrame* process(AudioFrame* frame) = 0;
    virtual void        setParam(const char* key, const char* value) = 0;
};

// ── Network source plugin ──────────────────────────────────────────────────────
class INetworkSourcePlugin : public IPlugin {
public:
    // Return FFmpeg-compatible URL (possibly with protocol options)
    virtual const char* resolveURL(const char* url) = 0;
    virtual bool        canHandle(const char* url)  = 0;
};

// ── AI model plugin ────────────────────────────────────────────────────────────
class IAIModelPlugin : public IPlugin {
public:
    enum class Task { Interpolation, Upscaling, Denoising, Classification };
    virtual Task        task()      const = 0;
    virtual bool        loadModel(const char* path) = 0;
    virtual VideoFrame* run(VideoFrame* f0, VideoFrame* f1, float t) = 0;
};

// ── Subtitle filter plugin ─────────────────────────────────────────────────────
struct SubtitleEvent {
    int64_t     startUs  = 0;   // microseconds
    int64_t     endUs    = 0;
    const char* text     = nullptr;
    const char* style    = nullptr; // ASS style string, may be null
};

class ISubtitleFilterPlugin : public IPlugin {
public:
    /// Called for each subtitle event; return modified copy or same pointer.
    virtual SubtitleEvent* process(SubtitleEvent* event) = 0;
    virtual void setParam(const char* key, const char* value) = 0;
};

} // namespace aurora::sdk

// ── Plugin export macros ──────────────────────────────────────────────────────
#ifdef _WIN32
#  define AURORA_EXPORT extern "C" __declspec(dllexport)
#else
#  define AURORA_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Plugins must export these three functions:
// AURORA_EXPORT aurora::sdk::PluginInfo aurora_plugin_info();
// AURORA_EXPORT aurora::sdk::IPlugin*   aurora_plugin_create();
// AURORA_EXPORT void                    aurora_plugin_destroy(aurora::sdk::IPlugin*);

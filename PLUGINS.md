# Plugin SDK — Aurora Motion Player

Aurora Motion Player exposes a C++ plugin API (`sdk/plugin/AuroraPlugin.h`) that allows
third-party developers to extend the player with custom video filters, audio filters,
subtitle filters, network sources, and AI models.

---

## Table of Contents

1. [Overview](#overview)
2. [Plugin Lifecycle](#plugin-lifecycle)
3. [Plugin Types](#plugin-types)
   - [Video Filter](#video-filter-plugin)
   - [Audio Filter](#audio-filter-plugin)
   - [Subtitle Filter](#subtitle-filter-plugin)
   - [Network Source](#network-source-plugin)
   - [AI Model](#ai-model-plugin)
4. [Required Exports](#required-exports)
5. [Building a Plugin](#building-a-plugin)
6. [Plugin Discovery](#plugin-discovery)
7. [Plugin Metadata](#plugin-metadata)
8. [Security & Sandbox](#security--sandbox)
9. [Complete Example](#complete-example)

---

## Overview

Plugins are **shared libraries** (`.dll` on Windows, `.so` on Android/Linux) placed in the
`plugins/` directory next to the Aurora executable. They are loaded at startup by
`PluginManager`. Every plugin implements at minimum the `IPlugin` base interface.

API version: **`AURORA_PLUGIN_API_VERSION 1`**

Header: `sdk/plugin/AuroraPlugin.h`

---

## Plugin Lifecycle

```
App starts
    │
    ▼
PluginManager::discoverPlugins()   ← scans plugins/ directory
    │
    ▼
Load shared library (.dll / .so)
    │
    ▼
Call aurora_plugin_info()          ← read PluginInfo, check API version
    │
    ▼
Call aurora_plugin_create()        ← instantiate plugin object
    │
    ▼
IPlugin::init()                    ← plugin initialises resources
    │
    ▼
[Plugin is active — callbacks fired during playback]
    │
    ▼
IPlugin::shutdown()                ← plugin releases resources
    │
    ▼
Call aurora_plugin_destroy()       ← delete plugin object
    │
    ▼
Unload shared library
```

---

## Plugin Types

```cpp
enum class PluginType {
    VideoFilter,
    AudioFilter,
    SubtitleFilter,
    NetworkSource,
    AIModel,
};
```

---

### Video Filter Plugin

Implement `IVideoFilterPlugin` to process raw decoded video frames.

```cpp
class IVideoFilterPlugin : public IPlugin {
public:
    // Process a video frame in-place or return a new frame.
    // Return the same pointer to modify in-place, or a new pointer to replace.
    virtual VideoFrame* process(VideoFrame* frame) = 0;
    virtual void        setParam(const char* key, const char* value) = 0;
    virtual const char* getParam(const char* key) = 0;
};
```

**`VideoFrame` fields:**

| Field | Type | Description |
|-------|------|-------------|
| `data[4]` | `uint8_t*` | Plane pointers (YUV or packed) |
| `linesize[4]` | `int` | Bytes per row per plane |
| `width` / `height` | `int` | Frame dimensions |
| `pts` | `int64_t` | Presentation timestamp (µs) |
| `pixFmt` | `int` | `AVPixelFormat` value |

---

### Audio Filter Plugin

Implement `IAudioFilterPlugin` to process interleaved float PCM audio.

```cpp
class IAudioFilterPlugin : public IPlugin {
public:
    virtual AudioFrame* process(AudioFrame* frame) = 0;
    virtual void        setParam(const char* key, const char* value) = 0;
};
```

**`AudioFrame` fields:**

| Field | Type | Description |
|-------|------|-------------|
| `samples` | `float*` | Interleaved float PCM samples |
| `nbSamples` | `int` | Samples per channel |
| `channels` | `int` | Channel count |
| `sampleRate` | `int` | Hz |
| `pts` | `int64_t` | Timestamp (µs) |

---

### Subtitle Filter Plugin

Implement `ISubtitleFilterPlugin` to intercept and modify subtitle events before rendering.

```cpp
class ISubtitleFilterPlugin : public IPlugin {
public:
    // Called for each SubtitleEvent. Return same pointer (modify in-place)
    // or a new SubtitleEvent to replace the original.
    virtual SubtitleEvent* process(SubtitleEvent* event) = 0;
    virtual void setParam(const char* key, const char* value) = 0;
};
```

**`SubtitleEvent` fields:**

| Field | Type | Description |
|-------|------|-------------|
| `startUs` | `int64_t` | Start time (µs) |
| `endUs` | `int64_t` | End time (µs) |
| `text` | `const char*` | Plain text or ASS dialogue line |
| `style` | `const char*` | ASS style override string (may be null) |

---

### Network Source Plugin

Implement `INetworkSourcePlugin` to add custom URL schemes or protocol resolvers.

```cpp
class INetworkSourcePlugin : public IPlugin {
public:
    // Returns an FFmpeg-compatible URL (with optional protocol options).
    virtual const char* resolveURL(const char* url) = 0;
    // Returns true if this plugin can handle the given URL.
    virtual bool        canHandle(const char* url)  = 0;
};
```

Example use-cases: OAuth-authenticated streams, proprietary CDN URLs, P2P sources.

---

### AI Model Plugin

Implement `IAIModelPlugin` to add custom inference models for interpolation, upscaling, or denoising.

```cpp
class IAIModelPlugin : public IPlugin {
public:
    enum class Task { Interpolation, Upscaling, Denoising, Classification };
    virtual Task        task()      const = 0;
    virtual bool        loadModel(const char* path) = 0;
    // For interpolation: f0 = frame A, f1 = frame B, t = blend factor [0,1]
    // For upscaling/denoising: f0 = input frame, f1 = nullptr, t = unused
    virtual VideoFrame* run(VideoFrame* f0, VideoFrame* f1, float t) = 0;
};
```

---

## Required Exports

Every plugin **must** export exactly these three C functions:

```cpp
AURORA_EXPORT aurora::sdk::PluginInfo aurora_plugin_info();
AURORA_EXPORT aurora::sdk::IPlugin*   aurora_plugin_create();
AURORA_EXPORT void                    aurora_plugin_destroy(aurora::sdk::IPlugin* plugin);
```

Use the `AURORA_EXPORT` macro (handles `__declspec(dllexport)` on Windows and
`__attribute__((visibility("default")))` on Linux/Android).

---

## Building a Plugin

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.27)
project(my_aurora_plugin)

add_library(my_aurora_plugin SHARED my_plugin.cpp)

target_include_directories(my_aurora_plugin PRIVATE
    /path/to/AuroraMotionPlayer/sdk/plugin
)

target_compile_features(my_aurora_plugin PRIVATE cxx_std_20)

# Windows: strip lib prefix
set_target_properties(my_aurora_plugin PROPERTIES PREFIX "")
```

### Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `my_aurora_plugin.dll` (Windows) or `my_aurora_plugin.so` (Android/Linux)

---

## Plugin Discovery

At startup `PluginManager` scans for plugin files:

```
Windows : <exe dir>\plugins\*.dll
Android : <app data dir>/plugins/*.so
```

Override the search directory via `--plugin-dir <path>` command-line argument
or the `aurora.pluginDir` setting in `aurora.ini`.

---

## Plugin Metadata

Fill in all `PluginInfo` fields:

```cpp
aurora::sdk::PluginInfo aurora_plugin_info() {
    return {
        .apiVersion  = AURORA_PLUGIN_API_VERSION,
        .type        = aurora::sdk::PluginType::VideoFilter,
        .name        = "My Brightness Filter",
        .version     = "1.0.0",
        .description = "Adjusts frame brightness",
        .author      = "Your Name",
        .license     = "MIT",
    };
}
```

Plugins with a mismatched `apiVersion` are skipped with a warning.

---

## Security & Sandbox

- Plugins run in the **main process** — a crashing plugin can crash the player
- Only load plugins from trusted sources
- Plugin isolation (separate process sandbox) is on the roadmap for a future release
- Safe Mode (`--safe-mode`) launches Aurora without loading any plugins

---

## Complete Example

See `sdk/plugin/example_brightness_plugin.cpp` for a working video filter plugin,
and `sdk/plugin/example_subtitle_censor.cpp` for a subtitle filter example.

**Minimal video filter skeleton:**

```cpp
#include "AuroraPlugin.h"
#include <cstring>

using namespace aurora::sdk;

class BrightnessFilter : public IVideoFilterPlugin {
    float m_brightness = 1.0f;

public:
    bool       init()     override { return true; }
    void       shutdown() override {}
    PluginInfo info() const override {
        return {AURORA_PLUGIN_API_VERSION, PluginType::VideoFilter,
                "Brightness", "1.0.0", "Adjust brightness", "You", "MIT"};
    }

    VideoFrame* process(VideoFrame* frame) override {
        if (!frame || !frame->data[0]) return frame;
        int total = frame->height * frame->linesize[0];
        for (int i = 0; i < total; ++i) {
            int v = static_cast<int>(frame->data[0][i] * m_brightness);
            frame->data[0][i] = static_cast<uint8_t>(v > 255 ? 255 : v);
        }
        return frame;
    }

    void        setParam(const char* key, const char* value) override {
        if (std::strcmp(key, "brightness") == 0)
            m_brightness = std::stof(value);
    }
    const char* getParam(const char* key) override { return nullptr; }
};

AURORA_EXPORT PluginInfo aurora_plugin_info()              { return BrightnessFilter{}.info(); }
AURORA_EXPORT IPlugin*   aurora_plugin_create()            { return new BrightnessFilter(); }
AURORA_EXPORT void       aurora_plugin_destroy(IPlugin* p) { delete p; }
```

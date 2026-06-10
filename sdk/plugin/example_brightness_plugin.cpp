/**
 * Example Aurora Plugin: Brightness/Contrast Video Filter
 *
 * Demonstrates how to implement an IVideoFilterPlugin.
 * Compile as a shared library and place in the plugins/ directory.
 *
 * Build (Linux):
 *   g++ -std=c++20 -shared -fPIC example_brightness_plugin.cpp \
 *       -I../../ -o aurora_brightness.so
 *
 * Build (Windows):
 *   cl /std:c++20 /LD example_brightness_plugin.cpp /Fe:aurora_brightness.dll
 */

#include "AuroraPlugin.h"
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <cstdio>

class BrightnessContrastPlugin : public aurora::sdk::IVideoFilterPlugin {
public:
    bool init() override {
        return true;
    }

    void shutdown() override {}

    aurora::sdk::PluginInfo info() const override {
        return {
            .apiVersion  = AURORA_PLUGIN_API_VERSION,
            .type        = aurora::sdk::PluginType::VideoFilter,
            .name        = "Brightness/Contrast",
            .version     = "1.0.0",
            .description = "Adjusts brightness and contrast of video frames",
            .author      = "Aurora Example",
            .license     = "MIT",
        };
    }

    aurora::sdk::VideoFrame* process(aurora::sdk::VideoFrame* frame) override {
        if (!frame || !frame->data[0]) return frame;

        int w = frame->width, h = frame->height;
        int ls = frame->linesize[0];

        // Apply brightness/contrast to Y (luma) plane only
        for (int y = 0; y < h; ++y) {
            uint8_t* row = frame->data[0] + y * ls;
            for (int x = 0; x < w; ++x) {
                float v = row[x];
                v = (v - 127.5f) * m_contrast + 127.5f + m_brightness;
                row[x] = static_cast<uint8_t>(
                    std::clamp(static_cast<int>(v), 0, 255));
            }
        }
        return frame;
    }

    void setParam(const char* key, const char* value) override {
        if (!key || !value) return;
        if (strcmp(key, "brightness") == 0) m_brightness = atof(value);
        if (strcmp(key, "contrast")   == 0) m_contrast   = atof(value);
    }

    const char* getParam(const char* key) override {
        static char buf[64];
        if (strcmp(key, "brightness") == 0) {
            snprintf(buf, sizeof(buf), "%.2f", m_brightness);
            return buf;
        }
        if (strcmp(key, "contrast") == 0) {
            snprintf(buf, sizeof(buf), "%.2f", m_contrast);
            return buf;
        }
        return nullptr;
    }

private:
    float m_brightness = 0.0f;   // -128 to +128
    float m_contrast   = 1.0f;   // 0.0 to 4.0
};

// ── Plugin exports ─────────────────────────────────────────────────────────────
AURORA_EXPORT aurora::sdk::PluginInfo aurora_plugin_info() {
    BrightnessContrastPlugin p;
    return p.info();
}

AURORA_EXPORT aurora::sdk::IPlugin* aurora_plugin_create() {
    return new BrightnessContrastPlugin();
}

AURORA_EXPORT void aurora_plugin_destroy(aurora::sdk::IPlugin* plugin) {
    delete plugin;
}

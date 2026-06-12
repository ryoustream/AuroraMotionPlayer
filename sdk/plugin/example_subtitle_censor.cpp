/**
 * Aurora Motion Player SDK — Example Subtitle Censor Plugin
 *
 * Demonstrates ISubtitleFilterPlugin by censoring a list of words.
 * Build as shared library and drop into the plugins/ directory.
 *
 * CMake example:
 *   add_library(aurora_subtitle_censor SHARED example_subtitle_censor.cpp)
 *   target_include_directories(aurora_subtitle_censor PRIVATE .)
 */

#include "AuroraPlugin.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace aurora::sdk;

class SubtitleCensorPlugin : public ISubtitleFilterPlugin {
public:
    bool init() override {
        // Default word list (can be overridden via setParam)
        m_words = {"badword1", "badword2"};
        return true;
    }

    void shutdown() override {
        m_words.clear();
    }

    PluginInfo info() const override {
        return {
            .apiVersion  = AURORA_PLUGIN_API_VERSION,
            .type        = PluginType::SubtitleFilter,
            .name        = "SubtitleCensor",
            .version     = "1.0.0",
            .description = "Censors configurable words in subtitles",
            .author      = "Aurora SDK Example",
            .license     = "MIT",
        };
    }

    SubtitleEvent* process(SubtitleEvent* event) override {
        if (!event || !event->text) return event;

        std::string text = event->text;
        bool changed = false;

        for (const auto& word : m_words) {
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            size_t pos = lower.find(word);
            while (pos != std::string::npos) {
                std::fill(text.begin() + pos,
                          text.begin() + pos + word.length(), '*');
                changed = true;
                pos = lower.find(word, pos + 1);
            }
        }

        if (changed) {
            m_modifiedText = text;
            event->text    = m_modifiedText.c_str();
        }
        return event;
    }

    void setParam(const char* key, const char* value) override {
        if (!key || !value) return;
        if (std::string(key) == "words") {
            // Comma-separated word list
            m_words.clear();
            std::string val = value;
            size_t start = 0, end;
            while ((end = val.find(',', start)) != std::string::npos) {
                auto word = val.substr(start, end - start);
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                m_words.push_back(word);
                start = end + 1;
            }
            auto last = val.substr(start);
            std::transform(last.begin(), last.end(), last.begin(), ::tolower);
            m_words.push_back(last);
        }
    }

private:
    std::vector<std::string> m_words;
    std::string              m_modifiedText;
};

// ── Plugin exports ────────────────────────────────────────────────────────────
AURORA_EXPORT PluginInfo aurora_plugin_info() {
    SubtitleCensorPlugin p;
    return p.info();
}

AURORA_EXPORT IPlugin* aurora_plugin_create() {
    return new SubtitleCensorPlugin();
}

AURORA_EXPORT void aurora_plugin_destroy(IPlugin* plugin) {
    delete plugin;
}

#pragma once
#include <string>
#include <map>
#include <cstdint>

namespace aurora::video {

struct VideoStream {
    int         index        = -1;
    std::string codecName;
    int         width        = 0;
    int         height       = 0;
    double      frameRate    = 0.0;
    int64_t     bitrate      = 0;
    double      duration     = 0.0;
    std::string pixFmt;
    std::string colorSpace;
    std::string colorTransfer;
    bool        isHDR        = false;
};

struct AudioStream {
    int         index      = -1;
    std::string codecName;
    int         channels   = 0;
    int         sampleRate = 0;
    int64_t     bitrate    = 0;
    std::string language;
    std::string title;
};

struct SubtitleStream {
    int         index    = -1;
    std::string codecName;
    std::string language;
    std::string title;
    bool        isForced = false;
};

struct ChapterInfo {
    int64_t     startPts = 0;
    int64_t     endPts   = 0;
    double      startTime = 0.0;
    double      endTime   = 0.0;
    std::string title;
};

struct VideoMetadata {
    std::string  filePath;
    std::string  formatName;
    double       duration    = 0.0;   // seconds
    int64_t      fileSize    = 0;     // bytes
    int64_t      totalBitrate = 0;

    std::vector<VideoStream>    videoStreams;
    std::vector<AudioStream>    audioStreams;
    std::vector<SubtitleStream> subtitleStreams;
    std::vector<ChapterInfo>    chapters;

    std::map<std::string, std::string> tags;

    // Convenience
    const VideoStream*    primaryVideo()    const;
    const AudioStream*    primaryAudio()    const;
    bool                  hasHDR()          const;
};

} // namespace aurora::video

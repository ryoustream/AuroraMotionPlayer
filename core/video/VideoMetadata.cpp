#include "VideoMetadata.h"

namespace aurora::video {

const VideoStream* VideoMetadata::primaryVideo() const {
    if (videoStreams.empty()) return nullptr;
    return &videoStreams[0];
}

const AudioStream* VideoMetadata::primaryAudio() const {
    if (audioStreams.empty()) return nullptr;
    return &audioStreams[0];
}

bool VideoMetadata::hasHDR() const {
    for (const auto& vs : videoStreams) {
        if (vs.isHDR) return true;
    }
    return false;
}

} // namespace aurora::video

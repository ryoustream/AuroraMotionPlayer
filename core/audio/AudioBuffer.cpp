#include "AudioBuffer.h"

namespace aurora::audio {

size_t AudioBuffer::bytesPerSample() const noexcept {
    switch (format) {
    case SampleFormat::U8:   return 1;
    case SampleFormat::S16:
    case SampleFormat::S16P: return 2;
    case SampleFormat::S32:
    case SampleFormat::FLT:
    case SampleFormat::FLTP: return 4;
    case SampleFormat::DBL:  return 8;
    default:                 return 2;
    }
}

size_t AudioBuffer::totalBytes() const noexcept {
    return static_cast<size_t>(nbSamples) * channels * bytesPerSample();
}

} // namespace aurora::audio

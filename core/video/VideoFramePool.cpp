#include "VideoFramePool.h"

namespace aurora::video {

VideoFramePool::VideoFramePool(size_t capacity, int width, int height, PixelFormat fmt)
    : m_capacity(capacity), m_width(width), m_height(height), m_format(fmt)
{
    m_storage.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
        auto frame = std::make_unique<VideoFrame>(width, height, fmt);
        m_free.push(frame.get());
        m_storage.push_back(std::move(frame));
    }
}

VideoFramePtr VideoFramePool::acquire() {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_free.empty(); });
    VideoFrame* raw = m_free.front();
    m_free.pop();
    return VideoFramePtr(raw, PoolDeleter{this});
}

VideoFramePtr VideoFramePool::tryAcquire(std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_mutex);
    if (!m_cv.wait_for(lock, timeout, [this] { return !m_free.empty(); }))
        return nullptr;
    VideoFrame* raw = m_free.front();
    m_free.pop();
    return VideoFramePtr(raw, PoolDeleter{this});
}

void VideoFramePool::release(VideoFrame* frame) {
    std::lock_guard lock(m_mutex);
    m_free.push(frame);
    m_cv.notify_one();
}

size_t VideoFramePool::available() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_free.size();
}

void VideoFramePool::flush() {
    // No-op: storage owns frames, pool is just a freelist
}

} // namespace aurora::video

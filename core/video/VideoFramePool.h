#pragma once
#include "VideoFrame.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace aurora::video {

// Thread-safe pool of pre-allocated VideoFrames for zero-copy pipeline
class VideoFramePool {
public:
    explicit VideoFramePool(size_t capacity, int width, int height, PixelFormat fmt);
    ~VideoFramePool() = default;

    // Acquire a frame from pool (blocks if empty)
    VideoFramePtr acquire();

    // Acquire with timeout (returns nullptr on timeout)
    VideoFramePtr tryAcquire(std::chrono::milliseconds timeout);

    // Release a frame back to pool (called automatically by shared_ptr deleter)
    void release(VideoFrame* frame);

    size_t capacity()  const noexcept { return m_capacity; }
    size_t available() const noexcept;
    void   flush();

private:
    struct PoolDeleter {
        VideoFramePool* pool = nullptr;
        void operator()(VideoFrame* f) const { if (pool) pool->release(f); }
    };

    size_t      m_capacity;
    int         m_width, m_height;
    PixelFormat m_format;

    mutable std::mutex          m_mutex;
    std::condition_variable     m_cv;
    std::queue<VideoFrame*>     m_free;
    std::vector<std::unique_ptr<VideoFrame>> m_storage;
};

} // namespace aurora::video

#pragma once
#include <string>
#include <functional>
#include <vector>
#include <cstdint>
#include <atomic>

namespace aurora::network {

// RTMP stream reader — wraps librtmp or FFmpeg RTMP input
class RTMPStream {
public:
    using DataCallback = std::function<void(const uint8_t*, size_t)>;

    RTMPStream() = default;
    ~RTMPStream();

    bool open(const std::string& rtmpUrl, int timeoutMs = 10000);
    void close();

    bool isConnected() const noexcept { return m_connected; }
    bool isLive()      const noexcept { return true; }  // RTMP is always live

    void setDataCallback(DataCallback cb) { m_dataCb = std::move(cb); }

    // Returns the FFmpeg-compatible URL string (rtmp:// passes through to FFmpeg)
    static std::string toFFmpegURL(const std::string& url) { return url; }

private:
    std::atomic<bool> m_connected{false};
    DataCallback      m_dataCb;
    std::string       m_url;
};

} // namespace aurora::network

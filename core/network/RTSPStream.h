#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <atomic>

namespace aurora::network {

enum class RTSPTransport {
    TCP,
    UDP,
    UDPMulticast,
};

class RTSPStream {
public:
    using DataCallback = std::function<void(const uint8_t*, size_t)>;

    RTSPStream() = default;
    ~RTSPStream();

    bool open(const std::string& rtspUrl,
              RTSPTransport transport = RTSPTransport::TCP,
              int timeoutMs = 10000);
    void close();

    bool isConnected() const noexcept { return m_connected; }

    void setDataCallback(DataCallback cb) { m_dataCb = std::move(cb); }

    static std::string toFFmpegURL(const std::string& url) { return url; }

private:
    std::atomic<bool> m_connected{false};
    DataCallback      m_dataCb;
    std::string       m_url;
};

} // namespace aurora::network

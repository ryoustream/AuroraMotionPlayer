#pragma once
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace aurora::network {

enum class Protocol {
    HTTP,
    HTTPS,
    HLS,
    DASH,
    RTMP,
    RTSP,
    SMB,
    FTP,
    WebDAV,
    Local,
};

struct NetworkConfig {
    int     connectTimeoutMs = 5000;
    int     readTimeoutMs    = 10000;
    int     bufferSizeKB     = 2048;
    int     maxRetriesOnError = 3;
    std::string userAgent    = "Aurora/1.0";
    std::string httpReferer;
    bool    enableCache      = true;
    bool    enableTLS        = true;
};

class NetworkSource {
public:
    using ProgressCallback = std::function<void(int64_t downloaded, int64_t total)>;
    using ErrorCallback    = std::function<void(const std::string&)>;

    explicit NetworkSource(NetworkConfig cfg = {});
    ~NetworkSource() = default;

    // Returns a URL suitable for passing to FFmpeg/mpv
    std::string resolveURL(const std::string& url);

    // Detect protocol from URL
    static Protocol detectProtocol(const std::string& url);
    static bool     isNetworkURL(const std::string& url);

    void setProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }
    void setErrorCallback(ErrorCallback cb)        { m_errorCb    = std::move(cb); }

    // Build FFmpeg AVOptions dict for the given URL
    std::string buildFFmpegOptions(const std::string& url) const;

    NetworkConfig& config() noexcept { return m_cfg; }

private:
    NetworkConfig     m_cfg;
    ProgressCallback  m_progressCb;
    ErrorCallback     m_errorCb;
};

} // namespace aurora::network

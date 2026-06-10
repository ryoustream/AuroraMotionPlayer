#include "NetworkSource.h"
#include <algorithm>
#include <cctype>

namespace aurora::network {

NetworkSource::NetworkSource(NetworkConfig cfg)
    : m_cfg(std::move(cfg))
{}

Protocol NetworkSource::detectProtocol(const std::string& url) {
    auto lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower.substr(0, 7) == "rtmp://" || lower.substr(0, 8) == "rtmps://") return Protocol::RTMP;
    if (lower.substr(0, 7) == "rtsp://") return Protocol::RTSP;
    if (lower.rfind(".m3u8") != std::string::npos)  return Protocol::HLS;
    if (lower.rfind(".mpd")  != std::string::npos)  return Protocol::DASH;
    if (lower.substr(0, 6) == "smb://")             return Protocol::SMB;
    if (lower.substr(0, 6) == "ftp://")             return Protocol::FTP;
    if (lower.substr(0, 8) == "webdav://")          return Protocol::WebDAV;
    if (lower.substr(0, 8) == "https://")           return Protocol::HTTPS;
    if (lower.substr(0, 7) == "http://")            return Protocol::HTTP;
    return Protocol::Local;
}

bool NetworkSource::isNetworkURL(const std::string& url) {
    return detectProtocol(url) != Protocol::Local;
}

std::string NetworkSource::resolveURL(const std::string& url) {
    // For most protocols, FFmpeg handles it natively
    // SMB/FTP/WebDAV may need libsmbclient or curl integration
    return url;
}

std::string NetworkSource::buildFFmpegOptions(const std::string& url) const {
    // Build key=value pairs understood by avformat_open_input options
    std::string opts;
    opts += "timeout="     + std::to_string(m_cfg.connectTimeoutMs * 1000); // us
    opts += ",rw_timeout=" + std::to_string(m_cfg.readTimeoutMs    * 1000);
    opts += ",user_agent=" + m_cfg.userAgent;
    if (!m_cfg.httpReferer.empty())
        opts += ",headers=Referer: " + m_cfg.httpReferer + "\\r\\n";
    if (detectProtocol(url) == Protocol::RTSP)
        opts += ",rtsp_transport=tcp";
    return opts;
}

} // namespace aurora::network

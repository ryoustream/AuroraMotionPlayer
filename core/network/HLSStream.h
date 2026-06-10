#pragma once
#include "NetworkSource.h"
#include <string>
#include <vector>
#include <functional>

namespace aurora::network {

// ── HLS Stream ────────────────────────────────────────────────────────────────
// HTTP Live Streaming (.m3u8) — handled natively by FFmpeg's HLS demuxer
class HLSStream {
public:
    struct Variant {
        int         bandwidth  = 0;      // bits/s
        int         width      = 0;
        int         height     = 0;
        std::string uri;
        std::string codecs;
    };

    // Parse master playlist and return variants sorted by bandwidth
    static std::vector<Variant> parseMasterPlaylist(const std::string& url);

    // Select best variant for given bandwidth (0 = auto)
    static Variant selectVariant(const std::vector<Variant>& variants,
                                  int targetBandwidth = 0);

    // Build FFmpeg-compatible URL with options for HLS
    static std::string buildURL(const std::string& masterURL,
                                 const NetworkConfig& cfg);
};

// ── DASH Stream ───────────────────────────────────────────────────────────────
// Dynamic Adaptive Streaming over HTTP (.mpd)
class DASHStream {
public:
    struct Representation {
        std::string id;
        int         bandwidth  = 0;
        int         width      = 0;
        int         height     = 0;
        std::string mimeType;
        std::string codecs;
    };

    static std::vector<Representation> parseMPD(const std::string& url);
    static std::string buildURL(const std::string& mpdURL,
                                 const NetworkConfig& cfg);
};

// ── RTMP Stream ───────────────────────────────────────────────────────────────
// Real-Time Messaging Protocol (live streaming)
class RTMPStream {
public:
    // Validate RTMP URL format
    static bool isValidURL(const std::string& url);

    // Build FFmpeg URL with RTMP options
    static std::string buildURL(const std::string& url,
                                 const NetworkConfig& cfg,
                                 const std::string& streamKey = "");

    // Parse stream key from URL (rtmp://host/app/streamkey)
    static std::string extractStreamKey(const std::string& url);
};

// ── RTSP Stream ───────────────────────────────────────────────────────────────
// Real Time Streaming Protocol (IP cameras, surveillance)
class RTSPStream {
public:
    enum class Transport { TCP, UDP, UDPMulticast };

    static std::string buildURL(const std::string& url,
                                 const NetworkConfig& cfg,
                                 Transport transport = Transport::TCP);

    // Check if URL is an RTSP stream
    static bool isValidURL(const std::string& url);
};

} // namespace aurora::network

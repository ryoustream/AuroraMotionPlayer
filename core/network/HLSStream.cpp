#include "HLSStream.h"
#include <algorithm>
#include <sstream>

namespace aurora::network {

// ── HLSStream ─────────────────────────────────────────────────────────────────
std::vector<HLSStream::Variant> HLSStream::parseMasterPlaylist(
    const std::string& /*url*/)
{
    // Full implementation: fetch URL with curl/FFmpeg avio and parse M3U8
    // Lines starting with #EXT-X-STREAM-INF contain bandwidth/resolution
    return {};
}

HLSStream::Variant HLSStream::selectVariant(
    const std::vector<Variant>& variants, int targetBandwidth)
{
    if (variants.empty()) return {};
    if (targetBandwidth <= 0) {
        // Auto: pick highest quality
        return *std::max_element(variants.begin(), variants.end(),
            [](const Variant& a, const Variant& b) {
                return a.bandwidth < b.bandwidth;
            });
    }
    // Pick highest variant below target
    Variant best = variants[0];
    for (const auto& v : variants) {
        if (v.bandwidth <= targetBandwidth && v.bandwidth > best.bandwidth)
            best = v;
    }
    return best;
}

std::string HLSStream::buildURL(const std::string& masterURL,
                                  const NetworkConfig& cfg)
{
    // For FFmpeg, HLS m3u8 URLs work directly
    // Add protocol options as AVOptions
    return masterURL;
}

// ── DASHStream ────────────────────────────────────────────────────────────────
std::vector<DASHStream::Representation> DASHStream::parseMPD(
    const std::string& /*url*/)
{
    // Full implementation: fetch MPD XML, parse AdaptationSets and Representations
    return {};
}

std::string DASHStream::buildURL(const std::string& mpdURL,
                                   const NetworkConfig& /*cfg*/)
{
    return mpdURL;  // FFmpeg handles DASH natively via libavformat
}

// ── RTMPStream ────────────────────────────────────────────────────────────────
bool RTMPStream::isValidURL(const std::string& url) {
    return url.substr(0, 7) == "rtmp://" ||
           url.substr(0, 8) == "rtmps://" ||
           url.substr(0, 8) == "rtmpe://" ||
           url.substr(0, 8) == "rtmpt://";
}

std::string RTMPStream::buildURL(const std::string& url,
                                   const NetworkConfig& cfg,
                                   const std::string& streamKey)
{
    std::string result = url;
    if (!streamKey.empty() && url.back() != '/')
        result += "/" + streamKey;
    // FFmpeg RTMP options can be appended as ?key=value pairs
    // or passed as AVOptions
    return result;
}

std::string RTMPStream::extractStreamKey(const std::string& url) {
    auto pos = url.rfind('/');
    return (pos != std::string::npos && pos + 1 < url.size())
           ? url.substr(pos + 1) : "";
}

// ── RTSPStream ────────────────────────────────────────────────────────────────
bool RTSPStream::isValidURL(const std::string& url) {
    return url.substr(0, 7) == "rtsp://" ||
           url.substr(0, 8) == "rtsps://";
}

std::string RTSPStream::buildURL(const std::string& url,
                                   const NetworkConfig& /*cfg*/,
                                   Transport transport)
{
    // FFmpeg uses rtsp_transport option
    // We pass this via av_dict_set in FFmpegDecoder::open()
    // The URL itself stays the same
    (void)transport;
    return url;
}

// ── RTMPStream / DASHStream / HLSStream stub CPPs ───────────────────────────
// These are in the same TU for now; split into separate files if needed

} // namespace aurora::network

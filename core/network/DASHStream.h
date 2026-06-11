#pragma once
#include "NetworkSource.h"
#include <string>
#include <vector>

namespace aurora::network {

struct DASHRepresentation {
    std::string id;
    int         bandwidth  = 0;
    int         width      = 0;
    int         height     = 0;
    std::string codecs;
    std::string mimeType;
    std::string baseURL;
    std::vector<std::string> segmentURLs;
};

class DASHStream {
public:
    DASHStream() = default;
    ~DASHStream() = default;

    bool open(const std::string& mpdUrl);
    void close();

    // Select representation (quality level)
    bool selectRepresentation(const std::string& id);
    bool selectBestRepresentation(int maxBandwidth = 0);  // 0 = unlimited

    const std::vector<DASHRepresentation>& representations() const noexcept
        { return m_reps; }

    std::string currentSegmentURL();
    double      segmentDuration() const noexcept { return m_segmentDuration; }
    bool        isLive() const noexcept { return m_isLive; }

private:
    bool parseMPD(const std::string& content);

    std::vector<DASHRepresentation> m_reps;
    int    m_selectedIdx     = 0;
    int    m_segmentIndex    = 0;
    double m_segmentDuration = 2.0;
    bool   m_isLive          = false;
};

} // namespace aurora::network

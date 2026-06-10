#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace aurora::video {

enum class PixelFormat {
    YUV420P,
    YUV422P,
    YUV444P,
    YUV420P10LE,
    YUV420P12LE,
    NV12,
    NV21,
    RGBA,
    BGRA,
    RGB24,
    P010LE,
    P016LE,
};

enum class ColorSpace {
    BT601,
    BT709,
    BT2020,
    SRGB,
};

enum class TransferFunction {
    BT709,
    SMPTE2084,  // PQ (HDR10)
    ARIB_STD_B67, // HLG
    LINEAR,
};

struct ColorMetadata {
    ColorSpace      colorSpace     = ColorSpace::BT709;
    TransferFunction transfer       = TransferFunction::BT709;
    bool            isHDR          = false;
    float           masterMaxLum   = 0.0f;  // nits
    float           masterMinLum   = 0.0f;
    float           maxContentLum  = 0.0f;
    float           maxFrameAvgLum = 0.0f;
};

class VideoFrame {
public:
    VideoFrame() = default;
    VideoFrame(int width, int height, PixelFormat fmt);
    ~VideoFrame() = default;

    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;
    VideoFrame(VideoFrame&&) noexcept = default;
    VideoFrame& operator=(VideoFrame&&) noexcept = default;

    // Accessors
    int          width()      const noexcept { return m_width; }
    int          height()     const noexcept { return m_height; }
    PixelFormat  format()     const noexcept { return m_format; }
    int64_t      pts()        const noexcept { return m_pts; }
    double       timeBase()   const noexcept { return m_timeBase; }
    bool         isHWFrame()  const noexcept { return m_isHW; }
    ColorMetadata colorMeta() const noexcept { return m_colorMeta; }

    uint8_t*     data(int plane = 0)    noexcept;
    const uint8_t* data(int plane = 0)  const noexcept;
    int          linesize(int plane = 0) const noexcept;

    void setPts(int64_t pts)              noexcept { m_pts = pts; }
    void setTimeBase(double tb)           noexcept { m_timeBase = tb; }
    void setColorMeta(ColorMetadata meta) noexcept { m_colorMeta = meta; }
    void setHWFrame(bool hw)              noexcept { m_isHW = hw; }

    // Timestamp helpers
    double timestampSeconds() const noexcept {
        return static_cast<double>(m_pts) * m_timeBase;
    }

private:
    int         m_width     = 0;
    int         m_height    = 0;
    PixelFormat m_format    = PixelFormat::YUV420P;
    int64_t     m_pts       = 0;
    double      m_timeBase  = 0.0;
    bool        m_isHW      = false;
    ColorMetadata m_colorMeta;

    // Planar data storage
    std::vector<uint8_t> m_planes[4];
    int                  m_linesizes[4] = {};
};

using VideoFramePtr = std::shared_ptr<VideoFrame>;

} // namespace aurora::video

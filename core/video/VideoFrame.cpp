#include "VideoFrame.h"
#include <stdexcept>

namespace aurora::video {

VideoFrame::VideoFrame(int width, int height, PixelFormat fmt)
    : m_width(width), m_height(height), m_format(fmt)
{
    // Allocate planes based on format
    switch (fmt) {
    case PixelFormat::YUV420P:
    case PixelFormat::YUV420P10LE:
    case PixelFormat::YUV420P12LE: {
        int bpp = (fmt == PixelFormat::YUV420P) ? 1 : 2;
        m_linesizes[0] = width * bpp;
        m_linesizes[1] = (width / 2) * bpp;
        m_linesizes[2] = (width / 2) * bpp;
        m_planes[0].resize(m_linesizes[0] * height);
        m_planes[1].resize(m_linesizes[1] * (height / 2));
        m_planes[2].resize(m_linesizes[2] * (height / 2));
        break;
    }
    case PixelFormat::YUV422P: {
        m_linesizes[0] = width;
        m_linesizes[1] = width / 2;
        m_linesizes[2] = width / 2;
        m_planes[0].resize(m_linesizes[0] * height);
        m_planes[1].resize(m_linesizes[1] * height);
        m_planes[2].resize(m_linesizes[2] * height);
        break;
    }
    case PixelFormat::YUV444P: {
        m_linesizes[0] = width;
        m_linesizes[1] = width;
        m_linesizes[2] = width;
        m_planes[0].resize(m_linesizes[0] * height);
        m_planes[1].resize(m_linesizes[1] * height);
        m_planes[2].resize(m_linesizes[2] * height);
        break;
    }
    case PixelFormat::NV12:
    case PixelFormat::NV21: {
        m_linesizes[0] = width;
        m_linesizes[1] = width;
        m_planes[0].resize(m_linesizes[0] * height);
        m_planes[1].resize(m_linesizes[1] * (height / 2));
        break;
    }
    case PixelFormat::RGBA:
    case PixelFormat::BGRA: {
        m_linesizes[0] = width * 4;
        m_planes[0].resize(m_linesizes[0] * height);
        break;
    }
    case PixelFormat::RGB24: {
        m_linesizes[0] = width * 3;
        m_planes[0].resize(m_linesizes[0] * height);
        break;
    }
    case PixelFormat::P010LE:
    case PixelFormat::P016LE: {
        m_linesizes[0] = width * 2;
        m_linesizes[1] = width * 2;
        m_planes[0].resize(m_linesizes[0] * height);
        m_planes[1].resize(m_linesizes[1] * (height / 2));
        break;
    }
    default:
        throw std::runtime_error("Unsupported pixel format");
    }
}

uint8_t* VideoFrame::data(int plane) noexcept {
    if (plane < 0 || plane >= 4 || m_planes[plane].empty()) return nullptr;
    return m_planes[plane].data();
}

const uint8_t* VideoFrame::data(int plane) const noexcept {
    if (plane < 0 || plane >= 4 || m_planes[plane].empty()) return nullptr;
    return m_planes[plane].data();
}

int VideoFrame::linesize(int plane) const noexcept {
    if (plane < 0 || plane >= 4) return 0;
    return m_linesizes[plane];
}

} // namespace aurora::video

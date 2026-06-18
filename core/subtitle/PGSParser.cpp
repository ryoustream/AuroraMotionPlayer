#include "PGSParser.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace aurora::subtitle {

// ── Endian helpers ────────────────────────────────────────────────────────────

uint16_t PGSParser::readU16BE(const uint8_t* p) noexcept {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
uint32_t PGSParser::readU24BE(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 16) |
           (static_cast<uint32_t>(p[1]) <<  8) |
            static_cast<uint32_t>(p[2]);
}
uint32_t PGSParser::readU32BE(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}

// ── YCbCr → RGBA ─────────────────────────────────────────────────────────────

void PGSParser::ycbcrToRGBA(const PaletteEntry& e,
                              uint8_t& r, uint8_t& g,
                              uint8_t& b, uint8_t& a) noexcept
{
    // BT.601 limited range
    float Y  = (e.Y  - 16)  * (255.0f / 219.0f);
    float Pb = (e.Cb - 128) * (255.0f / 224.0f);
    float Pr = (e.Cr - 128) * (255.0f / 224.0f);

    float rf = Y + 1.402f   * Pr;
    float gf = Y - 0.344f   * Pb - 0.714f * Pr;
    float bf = Y + 1.772f   * Pb;

    auto clamp = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v)));
    };

    r = clamp(rf); g = clamp(gf); b = clamp(bf); a = e.A;
}

// ── RLE decoder ───────────────────────────────────────────────────────────────
// PGS uses a 2-byte RLE scheme:
//   0x00 0x00          → end of line
//   0x00 0xXX (X≠0)   → X transparent pixels
//   0x00 0x4X ...      → run follows (14-bit count from 0x3F & next byte)
//   otherwise          → palette index byte, count=1

bool PGSParser::decodeRLE(const std::vector<uint8_t>& rle,
                            int w, int h,
                            const Palette& pal,
                            std::vector<uint8_t>& rgba)
{
    rgba.assign(static_cast<size_t>(w * h * 4), 0);

    int x = 0, y = 0;
    size_t i = 0;
    const size_t N = rle.size();

    auto putPixels = [&](uint8_t idx, int count) {
        uint8_t r, g, b, a;
        ycbcrToRGBA(pal.entries[idx], r, g, b, a);
        while (count-- > 0 && y < h) {
            size_t off = static_cast<size_t>((y * w + x) * 4);
            if (off + 3 < rgba.size()) {
                rgba[off]   = r;
                rgba[off+1] = g;
                rgba[off+2] = b;
                rgba[off+3] = a;
            }
            if (++x >= w) { x = 0; ++y; }
        }
    };

    while (i < N && y < h) {
        uint8_t b0 = rle[i++];
        if (b0 != 0x00) {
            // Single pixel at palette index b0
            putPixels(b0, 1);
        } else {
            if (i >= N) break;
            uint8_t b1 = rle[i++];
            if (b1 == 0x00) {
                // End of line — move to next row
                x = 0; ++y;
            } else if (b1 < 0x40) {
                // b1 transparent pixels
                putPixels(0, b1);
            } else if (b1 < 0x80) {
                // Long run of transparent pixels: count = ((b1 & 0x3F) << 8) | next
                if (i >= N) break;
                int cnt = ((b1 & 0x3F) << 8) | rle[i++];
                putPixels(0, cnt);
            } else if (b1 < 0xC0) {
                // b1 & 0x3F pixels of palette entry = next byte
                if (i >= N) break;
                uint8_t col = rle[i++];
                putPixels(col, b1 & 0x3F);
            } else {
                // Long run: count = ((b1 & 0x3F) << 8) | next; colour = next
                if (i + 1 >= N) break;
                int   cnt = ((b1 & 0x3F) << 8) | rle[i++];
                uint8_t col = rle[i++];
                putPixels(col, cnt);
            }
        }
    }
    return true;
}

// ── Segment readers ───────────────────────────────────────────────────────────

PGSParser::PCS PGSParser::readPCS(const uint8_t* d, size_t len)
{
    PCS pcs;
    if (len < 11) return pcs;

    pcs.width        = readU16BE(d + 0);
    pcs.height       = readU16BE(d + 2);
    pcs.frameRate    = d[4];
    pcs.compNumber   = readU16BE(d + 5);
    pcs.compState    = d[7];
    pcs.paletteUpdate= (d[8] & 0x80) != 0;
    pcs.paletteId    = d[9];
    uint8_t numObjs  = d[10];

    size_t off = 11;
    for (int i = 0; i < numObjs && off + 8 <= len; ++i) {
        CompositionObject co;
        co.objectId = readU16BE(d + off);
        co.windowId = d[off + 2];
        co.cropped  = (d[off + 3] & 0x80) != 0;
        co.x        = readU16BE(d + off + 4);
        co.y        = readU16BE(d + off + 6);
        off += 8;
        if (co.cropped && off + 8 <= len) {
            co.cropX = readU16BE(d + off);
            co.cropY = readU16BE(d + off + 2);
            co.cropW = readU16BE(d + off + 4);
            co.cropH = readU16BE(d + off + 6);
            off += 8;
        }
        pcs.objects.push_back(co);
    }
    return pcs;
}

PGSParser::Palette PGSParser::readPDS(const uint8_t* d, size_t len)
{
    Palette pal;
    if (len < 2) return pal;
    pal.id      = d[0];
    pal.version = d[1];
    size_t off = 2;
    while (off + 4 < len) {
        uint8_t idx   = d[off++];
        pal.entries[idx].Y  = d[off++];
        pal.entries[idx].Cr = d[off++];
        pal.entries[idx].Cb = d[off++];
        pal.entries[idx].A  = d[off++];
    }
    return pal;
}

PGSParser::ObjectData PGSParser::readODS(const uint8_t* d, size_t len)
{
    ObjectData obj;
    if (len < 4) return obj;
    obj.id      = readU16BE(d + 0);
    obj.version = d[2];
    uint8_t seqFlag = d[3];

    // First sequence: header present
    if (seqFlag & 0x80) {
        if (len < 11) return obj;
        obj.dataLen = readU24BE(d + 4);
        obj.width   = readU16BE(d + 7);
        obj.height  = readU16BE(d + 9);
        // RLE data starts at byte 11
        if (len > 11)
            obj.rleData.insert(obj.rleData.end(), d + 11, d + len);
    } else {
        // Continuation
        if (len > 4)
            obj.rleData.insert(obj.rleData.end(), d + 4, d + len);
    }

    obj.complete = (seqFlag & 0x40) != 0;  // Last in sequence
    return obj;
}

std::vector<PGSParser::WindowDef> PGSParser::readWDS(const uint8_t* d, size_t len)
{
    std::vector<WindowDef> wins;
    if (len < 1) return wins;
    uint8_t count = d[0];
    size_t  off   = 1;
    for (int i = 0; i < count && off + 9 <= len; ++i) {
        WindowDef w;
        w.id     = d[off];
        w.x      = readU16BE(d + off + 1);
        w.y      = readU16BE(d + off + 3);
        w.width  = readU16BE(d + off + 5);
        w.height = readU16BE(d + off + 7);
        off += 9;
        wins.push_back(w);
    }
    return wins;
}

// ── Display set compositor ────────────────────────────────────────────────────

SubtitleEvent PGSParser::compositeDisplaySet(
    const DisplaySet& ds, const DisplaySet& endDs)
{
    SubtitleEvent ev;
    ev.startTime = ds.pts;
    ev.endTime   = endDs.pts > ds.pts ? endDs.pts : ds.pts + 5.0; // default 5 s

    if (ds.pcs.objects.empty()) return ev;
    const auto& co = ds.pcs.objects[0];

    // Find matching object data
    const ObjectData* obj = nullptr;
    for (const auto& o : ds.objects)
        if (o.id == co.objectId) { obj = &o; break; }

    if (!obj || obj->width == 0 || obj->height == 0) return ev;

    std::vector<uint8_t> rgba;
    if (!decodeRLE(obj->rleData, obj->width, obj->height, ds.palette, rgba))
        return ev;

    ev.bitmap  = std::move(rgba);
    ev.bitmapW = obj->width;
    ev.bitmapH = obj->height;
    ev.bitmapX = co.x;
    ev.bitmapY = co.y;
    return ev;
}

// ── Stream parser ─────────────────────────────────────────────────────────────

PGSParser::DisplaySet PGSParser::readDisplaySet(
    const uint8_t*& p, const uint8_t* end)
{
    DisplaySet ds;

    while (p + 10 < end) {
        // Packet header: 0x50 0x47 magic
        if (p[0] != 0x50 || p[1] != 0x47) { ++p; continue; }

        uint32_t pts_raw = readU32BE(p + 2);
        /*uint32_t dts_raw = readU32BE(p + 6);*/
        uint8_t  segType = p[10];
        uint16_t segLen  = readU16BE(p + 11);
        p += 13;

        if (p + segLen > end) break;

        ds.pts = ptsTick(pts_raw);

        switch (segType) {
            case 0x16:  // PCS
                ds.pcs     = readPCS(p, segLen);
                break;
            case 0x17:  // WDS
                ds.windows = readWDS(p, segLen);
                break;
            case 0x14:  // PDS
                ds.palette = readPDS(p, segLen);
                break;
            case 0x15: { // ODS
                ObjectData od = readODS(p, segLen);
                // Merge continuation
                bool found = false;
                for (auto& ex : ds.objects) {
                    if (ex.id == od.id) {
                        ex.rleData.insert(ex.rleData.end(),
                                          od.rleData.begin(), od.rleData.end());
                        if (od.complete) ex.complete = true;
                        if (od.width)  { ex.width  = od.width;  }
                        if (od.height) { ex.height = od.height; }
                        found = true; break;
                    }
                }
                if (!found) ds.objects.push_back(std::move(od));
                break;
            }
            case 0x80:  // END
                ds.valid = true;
                p += segLen;
                return ds;
        }
        p += segLen;
    }
    return ds;
}

// ── Public API ────────────────────────────────────────────────────────────────

std::vector<SubtitleEvent> PGSParser::parseBuffer(
    const uint8_t* data, size_t size, double /*basePts*/)
{
    std::vector<SubtitleEvent> events;
    const uint8_t* p   = data;
    const uint8_t* end = data + size;

    DisplaySet prev;
    bool hasPrev = false;

    while (p < end) {
        DisplaySet ds = readDisplaySet(p, end);
        if (!ds.valid) break;

        if (hasPrev) {
            // prev = start display set, ds = end display set (empty PCS)
            SubtitleEvent ev = compositeDisplaySet(prev, ds);
            if (ev.bitmapW > 0)
                events.push_back(std::move(ev));
            hasPrev = false;
        }

        // A non-empty composition starts a new subtitle
        if (!ds.pcs.objects.empty()) {
            prev    = std::move(ds);
            hasPrev = true;
        }
    }
    return events;
}

std::vector<SubtitleEvent> PGSParser::parseFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};

    std::vector<uint8_t> buf(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());

    return parseBuffer(buf.data(), buf.size());
}

} // namespace aurora::subtitle

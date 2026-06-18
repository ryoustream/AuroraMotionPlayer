#pragma once
#include "SubtitleEngine.h"
#include <vector>
#include <cstdint>
#include <string>

namespace aurora::subtitle {

/**
 * Aurora Motion Player — PGS / SUP Parser (Session 8)
 *
 * Parses Presentation Graphic Stream (PGS) subtitle files (.sup / .m2ts embedded).
 *
 * PGS packet types:
 *   0x14  PDS  — Palette Definition Segment
 *   0x15  ODS  — Object Definition Segment  (RLE-compressed bitmap)
 *   0x16  PCS  — Presentation Composition Segment
 *   0x17  WDS  — Window Definition Segment
 *   0x80  END  — End of Display Set
 *
 * Output: vector<SubtitleEvent> with decoded RGBA bitmaps (bitmapX/Y/W/H/bitmap).
 */
class PGSParser {
public:
    // ── Segment structs ───────────────────────────────────────────────────────

    struct PaletteEntry {
        uint8_t Y  = 16;
        uint8_t Cr = 128;
        uint8_t Cb = 128;
        uint8_t A  = 255;
    };

    struct Palette {
        uint8_t       id      = 0;
        uint8_t       version = 0;
        PaletteEntry  entries[256]{};
    };

    struct ObjectData {
        uint16_t              id      = 0;
        uint8_t               version = 0;
        uint32_t              dataLen = 0;   // uncompressed size
        uint16_t              width   = 0;
        uint16_t              height  = 0;
        std::vector<uint8_t>  rleData;       // accumulated RLE payload
        bool                  complete = false;
    };

    struct CompositionObject {
        uint16_t objectId   = 0;
        uint8_t  windowId   = 0;
        bool     cropped    = false;
        uint16_t x          = 0;
        uint16_t y          = 0;
        uint16_t cropX      = 0;
        uint16_t cropY      = 0;
        uint16_t cropW      = 0;
        uint16_t cropH      = 0;
    };

    struct PCS {
        uint16_t width          = 1920;
        uint16_t height         = 1080;
        uint8_t  frameRate      = 0x10;
        uint16_t compNumber     = 0;
        uint8_t  compState      = 0;  // 0x00 Normal, 0x40 Acquisition, 0x80 Epoch
        bool     paletteUpdate  = false;
        uint8_t  paletteId      = 0;
        std::vector<CompositionObject> objects;
    };

    struct WindowDef {
        uint8_t  id     = 0;
        uint16_t x      = 0;
        uint16_t y      = 0;
        uint16_t width  = 0;
        uint16_t height = 0;
    };

    struct DisplaySet {
        double                      pts   = 0.0;
        double                      dts   = 0.0;
        PCS                         pcs;
        std::vector<WindowDef>      windows;
        Palette                     palette;
        std::vector<ObjectData>     objects;
        bool                        valid = false;
    };

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Parse a .sup file from disk.
     * Returns decoded SubtitleEvents with RGBA bitmaps.
     */
    static std::vector<SubtitleEvent> parseFile(const std::string& path);

    /**
     * Parse PGS data from a raw byte buffer (e.g. from FFmpeg subtitle stream).
     */
    static std::vector<SubtitleEvent> parseBuffer(
        const uint8_t* data, size_t size, double basePts = 0.0);

    /**
     * Decode a single RLE-encoded PGS object into an RGBA pixel buffer.
     * @param rle    Raw RLE bytes from ODS
     * @param w      Object width
     * @param h      Object height
     * @param pal    Palette to use for colorisation
     * @param rgba   Output RGBA buffer (w * h * 4 bytes)
     * @return true on success
     */
    static bool decodeRLE(
        const std::vector<uint8_t>& rle,
        int w, int h,
        const Palette& pal,
        std::vector<uint8_t>& rgba);

    /**
     * Convert YCbCr + Alpha palette entry to RGBA.
     */
    static void ycbcrToRGBA(const PaletteEntry& e,
                              uint8_t& r, uint8_t& g,
                              uint8_t& b, uint8_t& a) noexcept;

private:
    // Internal: parse one display set from a stream
    static DisplaySet readDisplaySet(const uint8_t*& p, const uint8_t* end);

    // Segment readers
    static PCS          readPCS(const uint8_t* d, size_t len);
    static Palette      readPDS(const uint8_t* d, size_t len);
    static ObjectData   readODS(const uint8_t* d, size_t len);
    static std::vector<WindowDef> readWDS(const uint8_t* d, size_t len);

    // Compose one SubtitleEvent from a complete display set
    static SubtitleEvent compositeDisplaySet(
        const DisplaySet& ds, const DisplaySet& endDs);

    // PTS/DTS are stored as 90 kHz ticks in the stream header
    static double ptsTick(uint32_t tick) noexcept { return tick / 90000.0; }
    static uint16_t readU16BE(const uint8_t* p) noexcept;
    static uint32_t readU24BE(const uint8_t* p) noexcept;
    static uint32_t readU32BE(const uint8_t* p) noexcept;
};

} // namespace aurora::subtitle

# Aurora Motion Player

<p align="center">
  <img src="docs/logo.png" alt="Aurora Motion Player" width="200"/>
</p>

<p align="center">
  <strong>AI-powered video player with real-time frame interpolation</strong><br/>
  Windows 10/11 x64 · Android 11+
</p>

<p align="center">
  <a href="../../actions/workflows/build-windows.yml">
    <img src="../../actions/workflows/build-windows.yml/badge.svg" alt="Build Windows"/>
  </a>
  <a href="../../actions/workflows/build-android.yml">
    <img src="../../actions/workflows/build-android.yml/badge.svg" alt="Build Android"/>
  </a>
  <a href="../../actions/workflows/ci.yml">
    <img src="../../actions/workflows/ci.yml/badge.svg" alt="CI"/>
  </a>
</p>

---

## Features

| Category | Features |
|----------|---------|
| **AI Interpolation** | RIFE · IFRNet · FILM · GMFlow — 24→60/120fps, 30→60/120fps |
| **AI Upscaling** | RealESRGAN · SPAN · Anime4K · FSRCNN — 2x / 4x / 8x |
| **HDR** | HDR10 · HDR10+ · HLG · Dolby Vision detection · BT.2390/ACES/Mobius/Reinhard tone mapping |
| **Decoders** | H.264 · H.265/HEVC · AV1 · VP9 · MPEG-2 · ProRes · DNxHD · MJPEG |
| **HW Decode** | NVDEC · Intel QSV · D3D11VA · DXVA2 · MediaCodec (Android) |
| **Audio** | AAC · FLAC · MP3 · AC3 · EAC3 · TrueHD · DTS · Opus · PCM + HDMI/SPDIF passthrough |
| **Subtitles** | SRT · ASS/SSA · VTT · PGS/SUP · IDX/SUB with custom fonts & animations |
| **Streaming** | HTTP · HLS · DASH · RTMP · RTSP · SMB · FTP · WebDAV |
| **Rendering** | Vulkan (primary) · DirectX 12 · DirectX 11 · OpenGL/ES |

**All features are free. No premium tier.**

---

## Architecture

```
AuroraPlayer/
├── core/                    # C++20 core engine (platform-agnostic)
│   ├── video/               # Frame types, pool, metadata
│   ├── audio/               # Audio engine, passthrough (HDMI/SPDIF)
│   ├── subtitle/            # SRT/ASS/VTT/PGS parser & renderer
│   ├── network/             # HTTP/HLS/DASH/RTMP/RTSP sources
│   ├── decoder/             # FFmpeg + HW accelerated decoder
│   ├── renderer/            # Vulkan & OpenGL renderers
│   ├── interpolation/       # AuroraFlow: RIFE/IFRNet/FILM/GMFlow
│   ├── upscaler/            # RealESRGAN/SPAN/Anime4K/FSRCNN
│   ├── hdr/                 # HDR engine + tone mappers
│   ├── scene/               # Scene detection & content classification
│   └── benchmark/           # FPS/CPU/GPU/VRAM metrics
├── desktop/windows/         # Qt6 Windows application
├── mobile/android/          # Kotlin + JNI Android application
├── tests/                   # Google Test unit tests
└── .github/workflows/       # GitHub Actions CI/CD
```

### AI Processing Pipeline

```
Decode (FFmpeg + HW Accel)
    ↓
Scene Detection  →  Auto model selection
    ↓
HDR Processing  (PQ/HLG → tone map if SDR display)
    ↓
AI Upscaling  (optional)
    ↓
AI Frame Interpolation  (RIFE / FILM / IFRNet / GMFlow)
    ↓
Render  (Vulkan / DX12 / DX11 / OpenGL)
```

---

## Building

### Windows

**Requirements:**
- Visual Studio 2022 (MSVC 17.x)
- CMake ≥ 3.25
- Qt 6.6+ (via `install-qt-action` in CI, or [qt.io](https://www.qt.io/download))
- Vulkan SDK 1.3+ ([lunarg.com](https://vulkan.lunarg.com/sdk/home))
- vcpkg (auto-bootstrapped in CI)

```powershell
# 1. Clone
git clone https://github.com/ryoustream/AuroraMotionPlayer.git
cd AuroraMotionPlayer

# 2. Bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
.\vcpkg\vcpkg.exe install ffmpeg:x64-windows

# 3. Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DAURORA_USE_VULKAN=ON -DAURORA_BUILD_TESTS=ON

# 4. Build
cmake --build build --config Release --parallel

# 5. Run tests
cd build && ctest -C Release --output-on-failure
```

### Android

**Requirements:**
- Android Studio / JDK 17
- Android NDK 26.1.10909125
- CMake 3.25+

```bash
cd mobile/android
./gradlew assembleRelease
# APK: app/build/outputs/apk/release/app-release.apk
```

---

## AI Models

Models are **not bundled** (size). Download separately and place in:

```
models/
├── rife/           rife.param + rife.bin  (NCNN)
├── rife/           rife.onnx              (ONNX Runtime)
├── ifrnet/         ifrnet.param + ifrnet.bin
├── film/           film.onnx
├── realesrgan/     realesrgan-x2.param + realesrgan-x2.bin
└── anime4k/        ...
```

Recommended sources:
- RIFE NCNN: [github.com/nihui/rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan/releases)
- RealESRGAN NCNN: [github.com/xinntao/Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan/releases)

---

## License

Aurora Motion Player is open source.

- **Core / UI code:** MIT License
- **FFmpeg:** LGPL 2.1+ (configured as LGPL — no GPL components enabled by default)
- **Qt6:** LGPL 3.0
- **NCNN:** BSD 3-Clause
- **ONNX Runtime:** MIT
- **AI model weights:** See individual model repos

---

## Minimum Hardware

| Platform | CPU | RAM | GPU |
|----------|-----|-----|-----|
| Windows  | Intel Core i5 Gen 8 / Ryzen 5 2600 | 8 GB | GTX 1650 / RX 570 / Intel Arc A380 |
| Android  | ARMv8 64-bit | 6 GB | Adreno 640 / Mali-G76 |

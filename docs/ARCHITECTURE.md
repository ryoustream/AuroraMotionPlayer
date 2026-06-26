# Aurora Motion Player — Architecture Guide

## Overview

Aurora is structured as a **layered architecture** with a platform-agnostic C++20 core
and platform-specific frontends for Windows (Qt6) and Android (Kotlin/JNI).

```
┌─────────────────────────────────────────────────────────┐
│                   Frontend Layer                        │
│  Qt6 Desktop (Windows)    │  Kotlin/JNI (Android)       │
├─────────────────────────────────────────────────────────┤
│                   Core Engine (C++20)                   │
│  Decoder │ Renderer │ Interpolation │ HDR │ Audio       │
├─────────────────────────────────────────────────────────┤
│                   Third-Party Libraries                 │
│  FFmpeg │ NCNN │ ONNX Runtime │ Vulkan │ Qt6            │
└─────────────────────────────────────────────────────────┘
```

---

## Video Pipeline

```
File/URL
  │
  ▼
NetworkSource  (resolve protocol: HTTP/HLS/DASH/RTMP/RTSP)
  │
  ▼
FFmpegDecoder  (demux + decode)
  │  ├── HW Accel: D3D11VA / NVDEC / QSV / MediaCodec
  │  └── SW fallback: multi-threaded FFmpeg
  │
  ▼ VideoFrame (YUV420P / YUV420P10 / NV12 / P010)
  │
  ▼
SceneDetector  (scene cut detection + content classification)
  │  └── Auto-selects interpolation model + upscaler
  │
  ▼
HDREngine  (PQ/HLG EOTF → tone mapping → BT.709 if SDR display)
  │  └── ToneMapper: BT.2390 / ACES / Mobius / Reinhard
  │
  ▼ (optional)
UpscalerBase  (RealESRGAN / SPAN / Anime4K / FSRCNN)
  │
  ▼
AuroraFlow (AI Frame Interpolation)
  │  ├── RIFE   (NCNN + Vulkan compute)
  │  ├── IFRNet (NCNN)
  │  ├── FILM   (ONNX Runtime + CUDA)
  │  └── GMFlow (NCNN)
  │
  ▼ Interpolated VideoFrame
  │
  ▼
RendererBase
  ├── VulkanRenderer    (primary — all platforms)
  ├── DX12Renderer      (Windows high-perf)
  ├── DX11Renderer      (Windows compat)
  └── OpenGLRenderer    (fallback / OpenGL ES Android)
```

---

## Threading Model

```
Main Thread (UI)
│
├── Decoder Thread Pool
│   ├── Demux Thread    — av_read_frame() → packet queues
│   ├── Video Decode    — avcodec_receive_frame() → frame queue
│   └── Audio Decode    — avcodec_receive_frame() → audio queue
│
├── AI Thread (GPU)
│   ├── Upscaler        — NCNN/ONNX inference
│   └── AuroraFlow      — Frame interpolation inference
│
├── Render Thread
│   └── Vulkan submit → present
│
└── Audio Thread
    └── AudioEngine → OS audio output
```

All queues are lock-free ring buffers or bounded blocking queues
with back-pressure to prevent memory unbounded growth.

---

## Memory Model

- `VideoFramePool`: Pre-allocated pool avoids per-frame malloc
- `AudioBuffer`: Pooled via shared_ptr with custom deleter
- GPU textures: Persistent upload buffer, mapped once
- AI inference: Tile-based processing for VRAM management

---

## Platform-Specific Notes

### Windows
- Renderer: Vulkan (primary) → D3D12 → D3D11 → OpenGL
- HW decode: D3D11VA (broadest support) → NVDEC → QSV
- Audio: WASAPI exclusive mode for minimal latency; SPDIF/HDMI passthrough
- File assoc: Shell extension registers MKV/MP4/AVI/etc.

### Android
- Renderer: Vulkan (Adreno 640+ / Mali-G76+) → OpenGL ES 3.2
- HW decode: MediaCodec (all devices) with async API
- Audio: AAudio (API 26+) for low-latency; HDMI via AudioTrack passthrough
- Storage: SAF (Storage Access Framework) for external files
- Background: Foreground service with MediaSession notification

---

## AI Model Integration

### NCNN Backend (primary)
- Models converted to NCNN format (.param + .bin)
- Vulkan compute for GPU acceleration on all Vulkan-capable GPUs
- Tile-based inference for VRAM-constrained devices (< 4 GB)

### ONNX Runtime Backend
- Models in .onnx format
- CUDA EP for NVIDIA GPUs
- DirectML EP for AMD/Intel on Windows
- CPU EP as universal fallback

### TensorRT Backend (optional)
- Highest performance on NVIDIA GPUs
- Requires model compilation (.trt engine)
- Platform: Windows only, requires CUDA toolkit

---

## Plugin System

Plugins are shared libraries (.dll/.so) discovered from the `plugins/` directory.
Each must export three C functions:

```cpp
AURORA_EXPORT PluginInfo aurora_plugin_info();
AURORA_EXPORT IPlugin*   aurora_plugin_create();
AURORA_EXPORT void       aurora_plugin_destroy(IPlugin*);
```

Plugin types: VideoFilter · AudioFilter · SubtitleFilter · NetworkSource · AIModel

Plugins run in isolated processes (sandbox) on Windows via Job Objects.
Crashes in plugins are caught and the plugin is disabled; the player continues.

---

## Build System

- **Windows**: CMake + Visual Studio 2022 generator, vcpkg for C++ deps, Qt installer
- **Android**: CMake + Ninja (via Gradle ExternalNativeBuild), NDK 26
- **CI/CD**: GitHub Actions — 4 workflows (build-windows, build-android, ci, release)
- **Tests**: Google Test, discovered via `gtest_discover_tests()`

---

## Session History

| Session | Theme | Key Components |
|---------|-------|---------------|
| S1 | Foundation | CMake scaffold, CI/CD, ToneMapper, SubtitleParser stubs |
| S2 | Rendering | VulkanRenderer, DX11Renderer, RendererFactory |
| S3 | Audio & Plugins | WASAPI, AAudio, DX12 compute, PluginManager |
| S4 | Interpolation | IFRNet, FILM, GMFlow, TileProcessor, InterpolationPipeline |
| S5 | Upscaling | RealESRGAN, SPAN, Anime4K, FSRCNN, UpscalerPipeline |
| S6 | Windows UI | ChapterWidget, BookmarkManager, ThumbnailBar, MiniMode, SpeedControl |
| S7 | Android UI | PlayerFragment, PiPManager, SubtitleFragment, UriUtils |
| S8 | Subtitle GPU | PGSParser, FreeTypeRenderer, ASSRenderer |
| S9 | Installer | WiX v4, Burn bootstrapper, build-installer.ps1 |
| S10 | Benchmark | GPUMonitor, CPUMonitor, BenchmarkOverlay, Android BenchmarkFragment |
| S11 | Integration Tests | test_Decoder/Pipeline/Subtitle/Network, download_models.py registry |
| S12 | Documentation | CHANGELOG, CONTRIBUTING, BUILD, MODELS, PLUGINS, PERFORMANCE, issue templates |

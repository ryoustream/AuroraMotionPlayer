# Changelog

All notable changes to **Aurora Motion Player** are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).  
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- TensorRT backend for NVIDIA GPU acceleration
- ROCm compute backend for AMD GPUs
- OpenVINO backend for Intel hardware
- Dolby Vision profile 5/8 tone mapping
- RTMP/RTSP live streaming ingest
- Plugin marketplace (in-app)

---

## [0.9.0] — Session 12 — 2025 (Documentation Release)

### Added — Documentation
- `CHANGELOG.md` — full project history (this file)
- `CONTRIBUTING.md` — contributor guide, code style, PR workflow
- `BUILD.md` — complete build instructions for Windows and Android
- `MODELS.md` — AI model registry, download instructions, benchmarks
- `PLUGINS.md` — plugin SDK reference, lifecycle, examples
- `PERFORMANCE.md` — tuning guide, hardware targets, benchmark methodology
- `docs/ARCHITECTURE.md` — updated with all Session 1–11 components
- GitHub issue templates — bug report, feature request, AI model request
- GitHub PR template

---

## [0.8.0] — Session 11 — Integration Tests & Model Registry

### Added
- Integration test suite: `test_Decoder`, `test_Pipeline`, `test_Subtitle`, `test_Network`
- Subtitle test fixtures (SRT, ASS, VTT, PGS)
- `models/download_models.py` — unified 10-model downloader with SHA-256 verify
- GitHub Actions workflow `integration-tests.yml`
- `tests/fixtures/` directory with sample media files

### Changed
- `ci.yml` — added integration-tests job

---

## [0.7.0] — Session 10 — GPU & CPU Benchmark System

### Added
- `GPUMonitor` — cross-platform GPU metrics (NVML · D3DKMT · kgsl · Mali · DRM)
- `CPUMonitor` — per-core utilisation, thread timing
- `BenchmarkOverlay` — real-time rolling graphs (FPS · VRAM · CPU · decode · interpolation)
- Android `BenchmarkFragment` + `BenchmarkViewModel`
- `benchmark.yml` GitHub Actions workflow

---

## [0.6.0] — Session 9 — Windows Installer

### Added
- WiX v4 installer (`installer/aurora.wxs`)
- Burn bootstrapper (`installer/aurora-bundle.wxs`)
- PowerShell build script `build-installer.ps1`
- Model download script `tools/scripts/download-models.py` / `.ps1` / `.sh`
- `build-installer.yml` GitHub Actions workflow
- `release.yml` updated with installer artifact upload

---

## [0.5.0] — Session 8 — Subtitle GPU Rendering

### Added
- `PGSParser` — Blu-ray PGS/SUP subtitle parser
- `FreeTypeRenderer` — GPU-accelerated glyph rasterization
- `ASSRenderer` — ASS/SSA animation engine
- Subtitle blend pass in Vulkan pipeline

---

## [0.4.0] — Session 7 — Android UI

### Added
- `PlayerFragment` — full-screen Kotlin player fragment
- `PiPManager` — Android Picture-in-Picture lifecycle
- `SubtitleFragment` — overlay subtitle rendering
- `UriUtils` — SAF / MediaStore URI resolution
- Android `build-android.yml` GitHub Actions workflow

---

## [0.3.0] — Session 6 — Windows Desktop UI

### Added
- `ChapterWidget` — chapter timeline bar
- `BookmarkManager` — persistent bookmark storage
- `ThumbnailBar` — seek-preview thumbnail strip
- `MiniModeWindow` — compact always-on-top window
- `SpeedControl` — variable playback speed UI (0.25×–4×)
- `FileAssociation` — Windows registry file-type binding
- `PlaybackHistory` — recent files with watch-progress

---

## [0.2.0] — Session 4 & 5 — AI Inference Pipelines

### Added — Interpolation (S4)
- `IFRNetInference` — IFRNet ONNX inference
- `FILMInference` — FILM frame interpolation
- `GMFlowInference` — GMFlow optical-flow inference
- `TileProcessor` — cosine-windowed tile overlap
- `InterpolationPipeline` — threaded multi-model pipeline, DXGI VRAM sharing

### Added — Upscaling (S5)
- `RealESRGANUpscaler`
- `SPANUpscaler`
- `Anime4KUpscaler`
- `FSRCNNUpscaler`
- `UpscalerPipeline` — model-select, tile, merge
- `ImageUtils` — pad / unpad / YUV↔RGB conversion

---

## [0.1.0] — Sessions 1–3 — Foundation

### Added — Session 1
- CMake/Ninja project scaffold with vcpkg
- `ToneMapper` (BT2390 · Mobius · ACES · Reinhard)
- `SubtitleParser` stubs (SRT · ASS · VTT)
- Android resource scaffold
- GitHub Actions `build-windows.yml`, `ci.yml`, `release.yml`

### Added — Session 2
- Vulkan renderer with SPIR-V pipeline
- DirectX 11 fallback renderer
- `RendererFactory` — runtime backend selection

### Added — Session 3
- WASAPI audio engine (Windows)
- AAudio audio engine (Android)
- DirectX 12 compute integration
- Plugin system (`PluginManager`, `IAuroraPlugin`)

---

[Unreleased]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.8.0...v0.9.0
[0.8.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/ryoustream/AuroraMotionPlayer/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/ryoustream/AuroraMotionPlayer/releases/tag/v0.1.0

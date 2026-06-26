# Performance Guide — Aurora Motion Player

This document covers hardware targets, performance tuning, benchmark methodology,
and tips for getting the best playback quality at the lowest latency.

---

## Table of Contents

1. [Minimum & Recommended Hardware](#minimum--recommended-hardware)
2. [AI Pipeline Performance](#ai-pipeline-performance)
3. [Tuning Playback Quality vs Speed](#tuning-playback-quality-vs-speed)
4. [HDR & Tone Mapping Performance](#hdr--tone-mapping-performance)
5. [Subtitle Rendering Performance](#subtitle-rendering-performance)
6. [Benchmark System](#benchmark-system)
7. [Profiling & Debugging](#profiling--debugging)
8. [Android-Specific Tips](#android-specific-tips)

---

## Minimum & Recommended Hardware

### Windows

| Tier | CPU | RAM | GPU | Target Use Case |
|------|-----|-----|-----|----------------|
| **Minimum** | Intel Core i5-8th gen / Ryzen 5 2600 | 8 GB | GTX 1650 / RX 570 / Intel Arc A380 | 1080p playback, RIFE Lite, 2× upscale |
| **Recommended** | Intel Core i7-12th gen / Ryzen 7 5700X | 16 GB | RTX 3060 / RX 6700 XT | 1080p→4K upscale, RIFE v4.6, 60fps interp |
| **High-end** | Intel Core i9-13th gen / Ryzen 9 7900X | 32 GB | RTX 4080 / RX 7900 XTX | 4K→8K, RIFE Ultra, multi-model concurrent |

### Android

| Tier | SoC | RAM | Target |
|------|-----|-----|--------|
| **Minimum** | Adreno 640 / Mali G76 | 6 GB | 1080p, basic upscale |
| **Recommended** | Adreno 730 / Mali G710 | 8 GB | 1080p→4K, RIFE Lite |
| **High-end** | Adreno 750 (Snapdragon 8 Gen 3) | 12 GB | 4K, RIFE v4.6 |

---

## AI Pipeline Performance

### Benchmark methodology

All benchmarks use:
- 1080p H.264 source (Big Buck Bunny, 24fps)
- Target: 60fps output
- GPU-only inference (no CPU fallback)
- Tile size: 256×256 with 32px overlap

Measurements are **throughput** (frames per second produced), not wall-clock playback speed.

### Frame Interpolation (24fps → 60fps, 1080p)

| Model | GTX 1650 4GB | RTX 3060 12GB | RTX 4080 16GB | Adreno 730 |
|-------|-------------|--------------|--------------|-----------|
| RIFE v4.14-lite | 48 fps | 140 fps | 310 fps | 38 fps |
| RIFE v4.6 | 32 fps | 95 fps | 220 fps | 25 fps |
| RIFE v4.20 | 18 fps | 55 fps | 130 fps | 14 fps |
| IFRNet-small | 28 fps | 82 fps | 190 fps | 20 fps |
| IFRNet-large | 15 fps | 45 fps | 105 fps | — |
| FILM-style | 10 fps | 32 fps | 80 fps | — |

> Values >24 fps mean real-time capable for 24fps source → 60fps target.

### Upscaling (1080p → 4K, single frame)

| Model | GTX 1650 4GB | RTX 3060 12GB | RTX 4080 16GB |
|-------|-------------|--------------|--------------|
| FSRCNN x4 | 55 fps | 160 fps | 380 fps |
| Anime4K-C | 48 fps | 140 fps | 330 fps |
| SPAN x4 | 28 fps | 90 fps | 210 fps |
| RealESRGAN x4plus | 12 fps | 42 fps | 105 fps |
| RealESRGAN anime_6B | 20 fps | 65 fps | 155 fps |

---

## Tuning Playback Quality vs Speed

### Interpolation Quality modes

| Mode | Model | Tile Size | Frame Queue | VRAM Budget |
|------|-------|-----------|-------------|-------------|
| Fast | RIFE v4.14-lite | 256 | 2 | ~512 MB |
| Balanced | RIFE v4.6 | 256 | 4 | ~1 GB |
| High | RIFE v4.6 + IFRNet-small | 512 | 6 | ~2 GB |
| Ultra | RIFE v4.20 + FILM-style | 512 | 8 | ~3.5 GB |

### Reducing dropped frames

1. **Lower tile size** — use 128 or 256 instead of 512 for weaker GPUs
2. **Reduce frame queue depth** — less VRAM buffering, lower latency
3. **Disable upscaling** while interpolating on < 4 GB VRAM GPUs
4. **Scene detection** — enable `aurora.sceneDetect=true`; bypasses interpolation at cuts
5. **Use NCNN backend** over ONNX for Vulkan GPUs (lower CPU overhead)

### Renderer selection priority

Aurora auto-selects the best available backend:

```
Vulkan  →  DirectX 12  →  DirectX 11  →  OpenGL ES (Android only)
```

Force a specific backend via `aurora.renderer=vulkan|dx12|dx11|opengl` in `aurora.ini`.

### Memory usage tips

| Setting | Default | Effect |
|---------|---------|--------|
| `aurora.frameCacheSize` | 8 | Reduce to 4 on < 8 GB RAM systems |
| `aurora.vramBudgetMB` | auto | Set to 80% of GPU VRAM |
| `aurora.tileOverlap` | 32 | Reduce to 16 for < 2 GB VRAM |
| `aurora.decoderThreads` | auto | Set to `nCores / 2` for power-efficient cores |

---

## HDR & Tone Mapping Performance

HDR tone mapping is a single GPU shader pass and adds < 0.5 ms per frame.

| Algorithm | Visual Characteristic | GPU Cost |
|-----------|----------------------|---------|
| BT.2390 | Reference display, most accurate | Low |
| Mobius | Preserves highlight structure | Low |
| ACES | Film-like roll-off, warm | Low |
| Reinhard | Simple, slightly desaturated highlights | Minimal |

**Dolby Vision** — profile detection only; full DV decode requires a licensed decoder.

**Auto HDR** — Windows Auto HDR is passed through if the display supports it.

---

## Subtitle Rendering Performance

- SRT/VTT: CPU-only, negligible cost
- ASS/SSA: FreeType GPU rasterization, < 1 ms per frame
- PGS (Blu-ray): GPU bitmap decode + alpha blend, < 2 ms per frame

### ASS acceleration

Set `aurora.subtitleGPU=true` (default) to render ASS/PGS glyphs on the GPU.
On < 2 GB VRAM GPUs, disable with `aurora.subtitleGPU=false` to save VRAM.

---

## Benchmark System

Aurora includes a built-in performance overlay. Enable with `Ctrl+B` or via the menu.

### Metrics displayed

| Metric | Description |
|--------|-------------|
| **Render FPS** | Frames composited to screen per second |
| **Decode FPS** | FFmpeg decoder throughput |
| **Interp FPS** | AI interpolation throughput |
| **GPU Usage** | GPU engine utilisation (%) |
| **VRAM Used** | Current GPU memory allocation |
| **CPU Usage** | Per-core utilisation |
| **Dropped Frames** | Frames skipped due to latency budget |

### Running the benchmark suite

```bash
# Build with benchmark support
cmake -B build -G Ninja -DAURORA_ENABLE_BENCHMARK=ON
cmake --build build --parallel

# GitHub Actions benchmark job runs automatically on push to main
# See .github/workflows/benchmark.yml
```

### Reading benchmark output

Benchmark results are written to `benchmark_results.json`:

```json
{
  "session": "rife_v4.6_1080p_60fps",
  "avg_render_fps": 62.4,
  "avg_interp_fps": 97.1,
  "avg_gpu_usage_pct": 78.2,
  "avg_vram_mb": 1842,
  "dropped_frames": 3,
  "total_frames": 3000
}
```

---

## Profiling & Debugging

### GPU profiling (Windows)

```powershell
# NVIDIA Nsight — attach to AuroraPlayer.exe
nsight-sys.exe --target-exe build\AuroraPlayer.exe

# RenderDoc — GPU frame capture
renderdoc.exe build\AuroraPlayer.exe
```

### CPU profiling

```powershell
# Visual Studio Diagnostic Tools
devenv /debugexe build\AuroraPlayer.exe
```

### Verbose logging

```bash
# Windows
.\build\AuroraPlayer.exe --log-level=debug --log-file=aurora.log

# Android
adb logcat -s AuroraPlayer:V
```

### Sanitizers (CI / development builds)

```bash
cmake -B build-san -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAURORA_SANITIZE=ON
cmake --build build-san --parallel
./build-san/aurora_tests
```

---

## Android-Specific Tips

### Background playback power saving

- Enable `aurora.android.lowPowerBackground=true` — halves AI inference frequency in background
- Disable upscaling in PiP mode automatically (enabled by default)

### Thermal throttling

Snapdragon and Dimensity chips throttle sustained GPU loads after ~10 minutes.
Aurora monitors thermal state via `/sys/class/thermal/thermal_zone*/temp` and
automatically reduces interpolation quality when the device approaches throttle thresholds.

### Storage performance

- **Internal NVMe:** Full 4K interpolation + upscaling supported
- **USB-C storage:** Decode only; AI inference reads from internal cache
- **SMB/NFS network:** Pre-buffer 8 seconds; use `aurora.networkBufferSec=8`

---

*For GPU-specific issues, check [GitHub Issues](https://github.com/ryoustream/AuroraMotionPlayer/issues)
filtered by `performance` label.*

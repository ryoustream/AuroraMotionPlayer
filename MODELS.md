# AI Model Registry — Aurora Motion Player

Aurora Motion Player uses open-source AI models for frame interpolation and super-resolution upscaling.
All models are licensed for free and commercial use.

---

## Table of Contents

1. [Quick Download](#quick-download)
2. [Frame Interpolation Models](#frame-interpolation-models)
3. [Super-Resolution / Upscaling Models](#super-resolution--upscaling-models)
4. [Model Selection Guide](#model-selection-guide)
5. [Adding Custom Models](#adding-custom-models)
6. [Model Directory Layout](#model-directory-layout)

---

## Quick Download

```bash
# Download all models
python3 models/download_models.py

# Download one specific model
python3 models/download_models.py --model rife
python3 models/download_models.py --model realesrgan

# List all available models with status
python3 models/download_models.py --list

# Verify SHA-256 checksums
python3 models/download_models.py --verify
```

**PowerShell:**

```powershell
.\tools\scripts\download-models.ps1 -ModelName all
```

---

## Frame Interpolation Models

### RIFE (Real-Time Intermediate Flow Estimation)

| Property | Value |
|----------|-------|
| Paper | [ECCV 2022](https://arxiv.org/abs/2011.06294) |
| License | MIT |
| Backend | NCNN |
| Formats | `.param` + `.bin` (NCNN) |
| Model key | `rife` |

**Variants available:**

| Variant | VRAM | Speed | Quality | Use Case |
|---------|------|-------|---------|----------|
| `rife-v4.6` | 512 MB | ★★★★★ | ★★★★ | Default — best speed/quality |
| `rife-v4.14-lite` | 256 MB | ★★★★★ | ★★★ | Low-VRAM devices |
| `rife-v4.20` | 1 GB | ★★★ | ★★★★★ | Ultra quality mode |

**Performance (RTX 3060, 1080p → 60fps):**
- `rife-v4.6`: ~95 fps throughput
- `rife-v4.14-lite`: ~140 fps throughput
- `rife-v4.20`: ~55 fps throughput

---

### IFRNet (Intermediate Feature Refine Network)

| Property | Value |
|----------|-------|
| Paper | [CVPR 2022](https://arxiv.org/abs/2205.14620) |
| License | Apache 2.0 |
| Backend | ONNX Runtime |
| Model key | `ifr` |

**Variants:**

| Variant | VRAM | Speed | Quality |
|---------|------|-------|---------|
| `ifrnet-large` | 1.2 GB | ★★★ | ★★★★★ |
| `ifrnet-small` | 512 MB | ★★★★ | ★★★★ |

---

### FILM (Frame Interpolation for Large Motion)

| Property | Value |
|----------|-------|
| Paper | [ECCV 2022](https://arxiv.org/abs/2202.04901) |
| License | Apache 2.0 |
| Backend | ONNX Runtime |
| Model key | `film` |

Best for large-motion content (sports, action). Less suited for anime.

| Variant | VRAM | Speed | Quality |
|---------|------|-------|---------|
| `film-style` | 1.5 GB | ★★ | ★★★★★ |

---

### GMFlow (Global Matching Flow)

| Property | Value |
|----------|-------|
| Paper | [CVPR 2022](https://arxiv.org/abs/2111.13680) |
| License | Apache 2.0 |
| Backend | ONNX Runtime |
| Model key | `gmflow` |

Optical-flow backbone used internally by the pipeline; not user-selectable directly.

---

## Super-Resolution / Upscaling Models

### RealESRGAN

| Property | Value |
|----------|-------|
| Paper | [ICCV 2021](https://arxiv.org/abs/2107.10833) |
| License | BSD 3-Clause |
| Backend | NCNN |
| Model key | `realesrgan` |

General-purpose upscaling. Excellent for live-action and animation.

| Variant | Scale | VRAM | Speed | Quality |
|---------|-------|------|-------|---------|
| `RealESRGAN_x4plus` | 4× | 2 GB | ★★★ | ★★★★★ |
| `RealESRGAN_x2plus` | 2× | 1 GB | ★★★★ | ★★★★★ |
| `RealESRGAN_x4plus_anime_6B` | 4× | 1 GB | ★★★★ | ★★★★★ |

---

### SPAN (Swift Parameter-free Attention Network)

| Property | Value |
|----------|-------|
| License | Apache 2.0 |
| Backend | NCNN |
| Model key | `span` |

Faster than RealESRGAN with comparable quality. Good for real-time 1080p→4K.

| Variant | Scale | VRAM | Speed |
|---------|-------|------|-------|
| `SPAN_x4` | 4× | 1.5 GB | ★★★★ |
| `SPAN_x2` | 2× | 768 MB | ★★★★★ |

---

### Anime4K

| Property | Value |
|----------|-------|
| License | MIT |
| Backend | Vulkan GLSL / NCNN |
| Model key | `anime4k` |

Optimised for anime content. Extremely fast (runs as a shader pass).

| Variant | Scale | VRAM | Speed |
|---------|-------|------|-------|
| `Anime4K-A` | 2× | 128 MB | ★★★★★ |
| `Anime4K-B` | 2× | 128 MB | ★★★★★ |
| `Anime4K-C` | 4× | 256 MB | ★★★★ |

---

### FSRCNN (Fast Super-Resolution CNN)

| Property | Value |
|----------|-------|
| Paper | [ECCV 2016](https://arxiv.org/abs/1608.00367) |
| License | MIT |
| Backend | NCNN |
| Model key | `fsrcnn` |

Lightweight upscaler for low-end hardware. Minimal VRAM usage.

| Variant | Scale | VRAM | Speed |
|---------|-------|------|-------|
| `FSRCNN_x2` | 2× | 64 MB | ★★★★★ |
| `FSRCNN_x3` | 3× | 64 MB | ★★★★★ |
| `FSRCNN_x4` | 4× | 64 MB | ★★★★★ |

---

## Model Selection Guide

### By content type

| Content | Recommended Interpolation | Recommended Upscaler |
|---------|--------------------------|---------------------|
| Anime | RIFE v4.6 | Anime4K-C or RealESRGAN anime_6B |
| Live action / movie | RIFE v4.20 or FILM | RealESRGAN x4plus |
| Sports / fast motion | FILM | SPAN x4 |
| Gaming / screen capture | RIFE v4.6 | FSRCNN x2 |
| Low-end hardware | RIFE v4.14-lite | FSRCNN x2 |

### By GPU VRAM

| VRAM | Interpolation | Upscaler |
|------|--------------|---------|
| < 2 GB | rife-v4.14-lite | FSRCNN x2 |
| 2–4 GB | rife-v4.6 | Anime4K or SPAN x2 |
| 4–8 GB | rife-v4.6 / ifrnet-small | RealESRGAN x2plus |
| 8 GB+ | rife-v4.20 / film-style | RealESRGAN x4plus |

---

## Adding Custom Models

Aurora supports custom NCNN and ONNX models via the plugin API.

### NCNN model

1. Export your model to NCNN format (`.param` + `.bin`)
2. Place in `models/<your-model-name>/model.param` + `model.bin`
3. Register via plugin:

```cpp
// In your plugin implementation
aurora::upscaler::NCNNUpscalerConfig cfg;
cfg.paramPath = "models/my-model/model.param";
cfg.binPath   = "models/my-model/model.bin";
cfg.scale     = 4;
cfg.tileSize  = 256;
pipeline->registerCustomUpscaler("my-model", cfg);
```

### ONNX model

1. Export to ONNX opset 17+
2. Place in `models/<your-model-name>/model.onnx`
3. Register via `ONNXInterpolatorConfig` in the plugin SDK

See [`PLUGINS.md`](PLUGINS.md) for the full plugin API.

---

## Model Directory Layout

```
models/
├── download_models.py          # Downloader script
├── rife/
│   ├── rife-v4.6.param
│   ├── rife-v4.6.bin
│   ├── rife-v4.14-lite.param
│   ├── rife-v4.14-lite.bin
│   └── rife-v4.20.param / .bin
├── ifr/
│   ├── ifrnet-large.onnx
│   └── ifrnet-small.onnx
├── film/
│   └── film-style.onnx
├── gmflow/
│   └── gmflow.onnx
├── realesrgan/
│   ├── RealESRGAN_x4plus.param / .bin
│   ├── RealESRGAN_x2plus.param / .bin
│   └── RealESRGAN_x4plus_anime_6B.param / .bin
├── span/
│   ├── SPAN_x4.param / .bin
│   └── SPAN_x2.param / .bin
├── anime4k/
│   ├── Anime4K-A.glsl
│   ├── Anime4K-B.glsl
│   └── Anime4K-C.glsl
└── fsrcnn/
    ├── FSRCNN_x2.param / .bin
    ├── FSRCNN_x3.param / .bin
    └── FSRCNN_x4.param / .bin
```

---

*All models are © their respective authors and licensed under the terms listed above.*  
*Aurora Motion Player does not redistribute model weights; they are downloaded at runtime.*

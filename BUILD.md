# Building Aurora Motion Player

This document covers building Aurora Motion Player from source on **Windows** (Desktop)
and **Android** (Mobile). CI builds run on GitHub Actions — see `.github/workflows/`.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Clone the Repository](#clone-the-repository)
3. [Windows Build](#windows-build)
4. [Android Build](#android-build)
5. [CMake Options](#cmake-options)
6. [Building the Windows Installer](#building-the-windows-installer)
7. [Downloading AI Models](#downloading-ai-models)
8. [Running Tests](#running-tests)
9. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Common

| Tool | Minimum Version | Notes |
|------|----------------|-------|
| Git | 2.40+ | |
| CMake | 3.27+ | |
| Ninja | 1.11+ | Build backend |
| Python | 3.10+ | Model download scripts |
| vcpkg | latest | C++ dependency manager |

### Windows

| Tool | Version | Download |
|------|---------|----------|
| Visual Studio 2022 | 17.8+ | C++ Desktop workload required |
| Windows SDK | 10.0.22621+ | Bundled with VS |
| Qt 6 | 6.6+ | [qt.io](https://www.qt.io/download) — MSVC 2022 x64 kit |
| Vulkan SDK | 1.3.268+ | [lunarg.com](https://vulkan.lunarg.com/) |
| WiX Toolset v4 | 4.0+ | `dotnet tool install --global wix` |

### Android

| Tool | Version | Notes |
|------|---------|-------|
| Android Studio | Hedgehog+ | Or standalone command-line tools |
| Android NDK | r26+ | Install via SDK Manager |
| Android SDK | API 30+ | Minimum target API 30 |
| Java | JDK 17 | Bundled with Android Studio |

---

## Clone the Repository

```bash
git clone https://github.com/ryoustream/AuroraMotionPlayer.git
cd AuroraMotionPlayer

# Initialize vcpkg submodule (if used as submodule)
git submodule update --init --recursive
```

---

## Windows Build

### 1. Bootstrap vcpkg

```powershell
# From repo root (PowerShell)
.\tools\bootstrap-vcpkg.ps1
# or manually:
cd third_party/vcpkg
.\bootstrap-vcpkg.bat -disableMetrics
cd ../..
```

### 2. Configure

```powershell
# Release build (recommended)
cmake -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DAURORA_BUILD_TESTS=ON `
    -DAURORA_ENABLE_VULKAN=ON `
    -DAURORA_ENABLE_DX11=ON `
    -DAURORA_ENABLE_DX12=ON

# Debug build
cmake -B build-debug -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DAURORA_BUILD_TESTS=ON
```

### 3. Build

```powershell
cmake --build build --parallel
```

Output artifacts:
- `build/AuroraPlayer.exe` — main application
- `build/aurora_core.dll` — core library
- `build/tests/` — test executables

### 4. Run

```powershell
.\build\AuroraPlayer.exe
```

---

## Android Build

### 1. Set environment variables

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk-r26
export ANDROID_SDK_ROOT=/path/to/android-sdk
```

### 2. Configure (CMake for NDK)

```bash
cmake -B build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-30 \
    -DCMAKE_BUILD_TYPE=Release \
    -DAURORA_PLATFORM=ANDROID
```

Supported ABIs: `arm64-v8a` (primary) · `x86_64` (emulator/testing)

### 3. Build core library

```bash
cmake --build build-android --parallel
```

### 4. Build APK (Gradle)

```bash
cd mobile/android
./gradlew assembleRelease
```

APK output: `mobile/android/app/build/outputs/apk/release/app-release.apk`

### 5. Install on device

```bash
adb install -r mobile/android/app/build/outputs/apk/release/app-release.apk
```

---

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `AURORA_BUILD_TESTS` | `OFF` | Build GTest unit & integration tests |
| `AURORA_ENABLE_VULKAN` | `ON` | Vulkan rendering backend |
| `AURORA_ENABLE_DX11` | `ON` | DirectX 11 fallback (Windows only) |
| `AURORA_ENABLE_DX12` | `ON` | DirectX 12 compute (Windows only) |
| `AURORA_ENABLE_NCNN` | `ON` | NCNN inference backend |
| `AURORA_ENABLE_ONNX` | `ON` | ONNX Runtime inference backend |
| `AURORA_ENABLE_CUDA` | `OFF` | NVIDIA CUDA acceleration |
| `AURORA_ENABLE_BENCHMARK` | `ON` | Build benchmark overlay |
| `AURORA_PLATFORM` | auto-detect | `WINDOWS` · `ANDROID` |
| `AURORA_SANITIZE` | `OFF` | Enable ASan + UBSan (debug builds) |
| `AURORA_COVERAGE` | `OFF` | Enable gcov coverage |

---

## Building the Windows Installer

Requires WiX Toolset v4 and .NET 6+ SDK.

```powershell
# Install WiX globally
dotnet tool install --global wix

# Build installer (after building the app)
.\build-installer.ps1 -BuildDir build -Version 0.9.0

# Output
#   installer/output/AuroraMotionPlayer-0.9.0-Setup.exe  (Burn bootstrapper)
#   installer/output/AuroraMotionPlayer-0.9.0.msi        (MSI component)
```

See `installer/README.md` for signing and notarization instructions.

---

## Downloading AI Models

Models are downloaded separately (not included in source). Use the Python script:

```bash
# Download all models
python3 models/download_models.py

# Download specific model
python3 models/download_models.py --model rife

# List available models
python3 models/download_models.py --list

# Verify existing downloads
python3 models/download_models.py --verify
```

Models are placed in `models/<name>/`. See [`MODELS.md`](MODELS.md) for the full registry.

**PowerShell alternative:**

```powershell
.\tools\scripts\download-models.ps1 -ModelName all
```

---

## Running Tests

### Unit tests

```bash
cd build
ctest --output-on-failure -L unit
```

### Integration tests

Integration tests require test fixtures:

```bash
python3 models/download_models.py --fixtures-only
cd build
ctest --output-on-failure -L integration
```

### With sanitizers

```bash
cmake -B build-san -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAURORA_SANITIZE=ON
cmake --build build-san --parallel
cd build-san && ctest --output-on-failure
```

---

## Troubleshooting

### vcpkg install fails

```powershell
# Clear vcpkg binary cache
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\vcpkg\archives"
# Retry
cmake --build build --target install
```

### Vulkan validation errors at startup

Install the Vulkan SDK and ensure `VK_LAYER_PATH` points to validation layers:

```powershell
$env:VK_LAYER_PATH = "C:\VulkanSDK\1.3.268\Bin"
.\build\AuroraPlayer.exe
```

### Android NDK not found

```bash
# Set NDK path explicitly in CMake
cmake ... -DANDROID_NDK=/explicit/path/to/ndk
```

### Qt not found

Set `Qt6_DIR` to the CMake config directory:

```powershell
cmake ... -DQt6_DIR="C:\Qt\6.6.0\msvc2022_64\lib\cmake\Qt6"
```

### Build fails on missing FFmpeg headers

Ensure vcpkg has installed FFmpeg:

```bash
vcpkg install ffmpeg:x64-windows
```

Or check that `VCPKG_TARGET_TRIPLET` matches your platform.

---

*For CI build logs, check the [GitHub Actions tab](https://github.com/ryoustream/AuroraMotionPlayer/actions).*

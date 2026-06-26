# Contributing to Aurora Motion Player

Thank you for your interest in contributing! Aurora Motion Player is an open-source project
and welcomes contributions of all kinds — bug reports, feature proposals, documentation
improvements, AI model integrations, and code.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [How to Contribute](#how-to-contribute)
4. [Branch & Commit Conventions](#branch--commit-conventions)
5. [Code Style](#code-style)
6. [Testing Requirements](#testing-requirements)
7. [Pull Request Process](#pull-request-process)
8. [Issue Labels](#issue-labels)
9. [Architecture Overview](#architecture-overview)

---

## Code of Conduct

This project follows the [Contributor Covenant v2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/).  
Be respectful, constructive, and inclusive.

---

## Getting Started

```bash
# 1. Fork the repository on GitHub, then clone your fork
git clone https://github.com/<your-username>/AuroraMotionPlayer.git
cd AuroraMotionPlayer

# 2. Add upstream remote
git remote add upstream https://github.com/ryoustream/AuroraMotionPlayer.git

# 3. Install build prerequisites (Windows)
#    See BUILD.md — Windows section

# 4. Install build prerequisites (Linux/CI)
sudo apt-get install -y cmake ninja-build pkg-config \
    libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev libswresample-dev libgtest-dev \
    clang-format-16

# 5. Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

---

## How to Contribute

### Reporting Bugs

Use the **Bug Report** issue template. Include:
- Aurora version (`VERSION` file)
- OS version and GPU
- Reproduction steps (minimal video file if possible)
- Logs from `%APPDATA%\Aurora\logs\` or `adb logcat`

### Requesting Features

Use the **Feature Request** template. Describe the use-case and expected behaviour.

### Contributing Code

1. Open an issue first for non-trivial changes so the approach can be discussed.
2. Create a feature branch from `main`.
3. Implement changes with tests.
4. Open a Pull Request against `main`.

### Contributing AI Models

Use the **AI Model Request** template. Models must be:
- Licensed for commercial use (Apache 2.0, MIT, or similar)
- Available as NCNN or ONNX weights
- Benchmarked against existing Aurora models

---

## Branch & Commit Conventions

### Branch naming

```
feat/<short-description>      # new feature
fix/<short-description>       # bug fix
docs/<short-description>      # documentation only
refactor/<short-description>  # code restructuring
test/<short-description>      # test additions
ci/<short-description>        # CI/CD changes
```

### Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

**Types:** `feat` · `fix` · `docs` · `style` · `refactor` · `test` · `ci` · `chore`

**Scopes:** `core` · `decoder` · `renderer` · `audio` · `subtitle` · `interpolation` ·
`upscaler` · `hdr` · `ui-windows` · `ui-android` · `benchmark` · `plugin` · `ci` · `docs`

**Examples:**

```
feat(interpolation): add GMFlow v2 NCNN backend
fix(subtitle): fix PGS palette overflow on 8-bit displays
docs(build): add vcpkg triplet instructions for ARM64
ci(android): cache Gradle dependencies between runs
```

---

## Code Style

### C++ (core library)

- **Standard:** C++20
- **Formatter:** `clang-format` with `.clang-format` at repo root (run before every commit)
- **Naming:**
  - Types / classes: `PascalCase`
  - Functions / methods: `camelCase`
  - Member variables: `m_camelCase`
  - Constants / enum values: `UPPER_SNAKE_CASE`
  - Namespaces: `snake_case` (`aurora::`, `aurora::decoder::`)
- **Headers:** use `#pragma once`
- **Includes:** group as system → third-party → aurora internal; separated by blank lines
- **Error handling:** return `std::expected<T, AuroraError>` or `bool`; no exceptions in hot paths
- **Platform guards:** use `#ifdef _WIN32` / `#ifdef __ANDROID__`; never assume OS in shared code

```cpp
// Good
namespace aurora::interpolation {

class RIFEInference {
public:
    bool        init(const std::string& modelPath);
    bool        interpolate(const Frame& f0, const Frame& f1, Frame& out, float t);
    void        shutdown();

private:
    ncnn::Net   m_net;
    bool        m_initialized{false};
};

} // namespace aurora::interpolation
```

### Kotlin (Android frontend)

- **Standard:** Kotlin 1.9+
- **Style:** [Android Kotlin Style Guide](https://developer.android.com/kotlin/style-guide)
- **Coroutines:** prefer `Flow` over callbacks; use `viewModelScope`
- **Jetpack:** use ViewModel, LiveData/StateFlow, Navigation component

### CMake

- Targets use `aurora_<module>` naming
- Use `target_compile_features(target PRIVATE cxx_std_20)`
- No global `include_directories()` — use target-scoped `target_include_directories()`

### YAML (GitHub Actions)

- Step names start with an emoji category glyph
- Use `timeout-minutes` on every job
- Pin action versions with full SHA where possible

---

## Testing Requirements

All new code must include tests:

| Layer | Framework | Location |
|-------|-----------|----------|
| C++ unit | GTest / GMock | `tests/unit/` |
| C++ integration | GTest | `tests/integration/` |
| Android UI | Espresso / JUnit4 | `mobile/android/app/src/androidTest/` |

### Running tests locally

```bash
# Unit tests (Linux/macOS)
cmake -B build -G Ninja -DAURORA_BUILD_TESTS=ON
cmake --build build --parallel
cd build && ctest --output-on-failure

# Integration tests (requires test fixtures)
python3 models/download_models.py --fixtures-only
ctest -R integration --output-on-failure
```

### Test conventions

- Tests live in a file named `test_<Module>.cpp`
- Fixtures that require external files use `GTEST_SKIP()` when absent (CI-safe)
- Mock hardware resources with GMock interfaces; never require real GPU in unit tests

---

## Pull Request Process

1. **Rebase** your branch on latest `main` before opening a PR:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **CI must pass** — all of: format-check · unit-tests · static-analysis · sanitizers

3. **PR description** must include:
   - What changed and why
   - How to test the change
   - Screenshots / videos for UI changes
   - Benchmark numbers for AI pipeline changes

4. **Review:** at least one maintainer approval required before merge

5. **Squash and merge** is the default strategy for feature branches

---

## Issue Labels

| Label | Meaning |
|-------|---------|
| `bug` | Confirmed defect |
| `feature` | New capability request |
| `ai-model` | New or improved AI model |
| `performance` | Speed / memory regression or improvement |
| `documentation` | Docs-only change |
| `good first issue` | Suitable for new contributors |
| `help wanted` | Maintainers need community help |
| `platform:windows` | Windows-specific |
| `platform:android` | Android-specific |
| `component:interpolation` | Frame interpolation subsystem |
| `component:subtitle` | Subtitle engine |
| `component:renderer` | GPU renderer |
| `needs-repro` | Bug lacking reproduction steps |
| `wontfix` | Out of scope |

---

## Architecture Overview

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for a full diagram.

Key layers:

```
core/decoder/         FFmpeg-based video & audio decoder
core/renderer/        Vulkan → DX12 → DX11 rendering backend
core/interpolation/   RIFE / IFRNet / FILM / GMFlow AI pipeline
core/upscaler/        RealESRGAN / SPAN / Anime4K / FSRCNN
core/subtitle/        SRT / ASS / PGS parser & GPU renderer
core/audio/           WASAPI (Windows) + AAudio (Android)
core/hdr/             HDR10 / HLG / DV tone mapping
core/benchmark/       GPU + CPU performance monitoring
desktop/windows/      Qt6 Windows UI
mobile/android/       Kotlin + JNI Android UI
sdk/plugin/           Third-party plugin API
```

---

*Happy hacking! — The Aurora Motion Player team*

#!/usr/bin/env bash
# Aurora Motion Player - Linux/macOS Build Script
# Usage: ./tools/scripts/build-linux.sh [--release|--debug] [--tests] [--clean]

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/build"
BUILD_TYPE="Release"
RUN_TESTS=0
CLEAN=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

log()  { echo -e "\033[36m[Aurora] $*\033[0m"; }
die()  { echo -e "\033[31m[ERROR] $*\033[0m" >&2; exit 1; }

# Parse args
for arg in "$@"; do
    case $arg in
        --release) BUILD_TYPE="Release"      ;;
        --debug)   BUILD_TYPE="Debug"        ;;
        --tests)   RUN_TESTS=1               ;;
        --clean)   CLEAN=1                   ;;
        --jobs=*)  JOBS="${arg#--jobs=}"     ;;
        *) die "Unknown argument: $arg"      ;;
    esac
done

log "Build type: $BUILD_TYPE | Jobs: $JOBS"

# Clean
if [ "$CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    log "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Check tools
command -v cmake  >/dev/null 2>&1 || die "CMake not found"
command -v ninja  >/dev/null 2>&1 || die "Ninja not found (apt install ninja-build)"

# Check FFmpeg
pkg-config --exists libavcodec 2>/dev/null || \
    die "FFmpeg not found. Install: apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev"

# Configure
log "Configuring CMake..."
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DAURORA_BUILD_DESKTOP=OFF \
    -DAURORA_BUILD_TESTS="$( [ "$RUN_TESTS" -eq 1 ] && echo ON || echo ON )" \
    -DAURORA_USE_VULKAN="$(pkg-config --exists vulkan 2>/dev/null && echo ON || echo OFF)" \
    "$ROOT"

# Build
log "Building with $JOBS jobs..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

log "Build succeeded!"

# Tests
if [ "$RUN_TESTS" -eq 1 ]; then
    log "Running tests..."
    cd "$BUILD_DIR"
    ctest --output-on-failure --timeout 60 -j4
    log "All tests passed!"
fi

log "Done."

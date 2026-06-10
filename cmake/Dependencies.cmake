# cmake/Dependencies.cmake
# Locate and configure all third-party dependencies

include(FetchContent)

# ── FFmpeg ───────────────────────────────────────────────────────────────────
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswscale
        libswresample
        libavfilter
    )
endif()

if(NOT FFMPEG_FOUND)
    message(STATUS "FFmpeg not found via pkg-config, using vcpkg or system paths")
    find_library(AVCODEC_LIB  avcodec  HINTS ${FFMPEG_PREFIX}/lib)
    find_library(AVFORMAT_LIB avformat HINTS ${FFMPEG_PREFIX}/lib)
    find_library(AVUTIL_LIB   avutil   HINTS ${FFMPEG_PREFIX}/lib)
    find_library(SWSCALE_LIB  swscale  HINTS ${FFMPEG_PREFIX}/lib)
    find_library(SWRESAMPLE_LIB swresample HINTS ${FFMPEG_PREFIX}/lib)
endif()

# ── Qt6 ──────────────────────────────────────────────────────────────────────
if(AURORA_BUILD_DESKTOP)
    find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Multimedia
        Network
        OpenGL
        Concurrent
        Svg
    )
    qt_standard_project_setup()
endif()

# ── Vulkan ───────────────────────────────────────────────────────────────────
if(AURORA_USE_VULKAN)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan found: ${Vulkan_VERSION}")
endif()

# ── NCNN ─────────────────────────────────────────────────────────────────────
find_package(ncnn QUIET HINTS ${NCNN_DIR})
if(NOT ncnn_FOUND)
    message(STATUS "NCNN not found in system, will use bundled")
endif()

# ── ONNX Runtime ─────────────────────────────────────────────────────────────
find_library(ONNXRUNTIME_LIB onnxruntime HINTS ${ONNXRUNTIME_DIR}/lib)
find_path(ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_DIR}/include
)

# ── DirectX (Windows only) ───────────────────────────────────────────────────
if(WIN32)
    if(AURORA_USE_DX12)
        find_library(D3D12_LIB d3d12)
        find_library(DXGI_LIB  dxgi)
    endif()
    if(AURORA_USE_DX11)
        find_library(D3D11_LIB d3d11)
    endif()
endif()

# ── libplacebo ────────────────────────────────────────────────────────────────
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPLACEBO IMPORTED_TARGET libplacebo>=6.0)
endif()

# ── CUDA / TensorRT ──────────────────────────────────────────────────────────
if(AURORA_USE_CUDA)
    find_package(CUDAToolkit REQUIRED)
endif()
if(AURORA_USE_TENSORRT)
    find_library(TENSORRT_LIB nvinfer HINTS ${TENSORRT_DIR}/lib)
    find_library(TENSORRT_ONNX_LIB nvonnxparser HINTS ${TENSORRT_DIR}/lib)
endif()

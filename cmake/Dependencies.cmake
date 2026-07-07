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
    message(STATUS "FFmpeg not found via pkg-config — trying FFMPEG_PREFIX=${FFMPEG_PREFIX}")
    find_library(AVCODEC_LIB    avcodec    HINTS ${FFMPEG_PREFIX}/lib)
    find_library(AVFORMAT_LIB   avformat   HINTS ${FFMPEG_PREFIX}/lib)
    find_library(AVUTIL_LIB     avutil     HINTS ${FFMPEG_PREFIX}/lib)
    find_library(AVFILTER_LIB   avfilter   HINTS ${FFMPEG_PREFIX}/lib)
    find_library(SWSCALE_LIB    swscale    HINTS ${FFMPEG_PREFIX}/lib)
    find_library(SWRESAMPLE_LIB swresample HINTS ${FFMPEG_PREFIX}/lib)

    if(AVCODEC_LIB AND AVFORMAT_LIB AND AVUTIL_LIB)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        # Only add explicit include dir if FFMPEG_PREFIX is set and dir exists
        if(FFMPEG_PREFIX AND EXISTS "${FFMPEG_PREFIX}/include")
            target_include_directories(FFmpeg::FFmpeg INTERFACE "${FFMPEG_PREFIX}/include")
        endif()
        target_link_libraries(FFmpeg::FFmpeg INTERFACE
            ${AVCODEC_LIB}
            ${AVFORMAT_LIB}
            ${AVUTIL_LIB}
            ${SWSCALE_LIB}
            ${SWRESAMPLE_LIB}
            $<$<BOOL:${AVFILTER_LIB}>:${AVFILTER_LIB}>
        )
        set(FFMPEG_FOUND TRUE)
        message(STATUS "FFmpeg found: AVCODEC=${AVCODEC_LIB} PREFIX=${FFMPEG_PREFIX}")
    else()
        message(WARNING "FFmpeg libraries not found — decoder will be stub only")
    endif()
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
    # qt_standard_project_setup() requires Qt >= 6.3; guard against older distro Qt
    if(Qt6_VERSION VERSION_GREATER_EQUAL "6.3.0")
        qt_standard_project_setup()
    endif()
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

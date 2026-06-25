cmake_minimum_required(VERSION 3.25)

# ── Aurora Core Library ───────────────────────────────────────────────────────
add_library(aurora_core STATIC
    # Video
    video/VideoFrame.cpp
    video/VideoFramePool.cpp
    video/VideoMetadata.cpp

    # Audio
    audio/AudioBuffer.cpp
    audio/AudioEngine.cpp
    audio/AudioPassthrough.cpp
    audio/AudioResampler.cpp

    # Subtitle
    subtitle/SubtitleEngine.cpp
    subtitle/SubtitleParser.cpp
    subtitle/SubtitleRenderer.cpp
    subtitle/PGSParser.cpp
    subtitle/FreeTypeRenderer.cpp
    subtitle/ASSRenderer.cpp

    # Network
    network/NetworkSource.cpp
    network/HLSStream.cpp
    network/DASHStream.cpp
    network/RTMPStream.cpp
    network/RTSPStream.cpp

    # Decoder
    decoder/FFmpegDecoder.cpp
    decoder/HardwareDecoder.cpp
    decoder/DecoderFactory.cpp

    # Renderer
    renderer/RendererBase.cpp
    renderer/VulkanRenderer.cpp
    renderer/OpenGLRenderer.cpp
    renderer/DX11Renderer.cpp
    renderer/RendererFactory.cpp

    # DX12 (Windows only)
    $<$<PLATFORM_ID:Windows>:renderer/DX12Renderer.cpp>

    # Plugin system
    plugin/PluginManager.cpp

    # AI Pipeline
    pipeline/AIPipelineManager.cpp

    # Interpolation
    interpolation/AuroraFlow.cpp
    interpolation/RIFEInterpolator.cpp
    interpolation/IFRNetInterpolator.cpp
    interpolation/FILMInterpolator.cpp
    interpolation/GMFlowInterpolator.cpp
    interpolation/MotionEstimator.cpp
    interpolation/MotionCompensator.cpp
    interpolation/TileProcessor.cpp
    interpolation/InterpolationPipeline.cpp

    # Upscaler
    upscaler/ImageUtils.cpp
    upscaler/RealESRGAN.cpp
    upscaler/SPAN.cpp
    upscaler/Anime4K.cpp
    upscaler/FSRCNN.cpp
    upscaler/UpscalerFactory.cpp
    upscaler/UpscalerPipeline.cpp

    # HDR
    hdr/HDREngine.cpp
    hdr/ToneMapper.cpp
    hdr/DolbyVisionParser.cpp

    # Scene Detection
    scene/SceneDetector.cpp
    scene/SceneClassifier.cpp

    # Benchmark
    benchmark/BenchmarkSystem.cpp
    benchmark/FrameCounter.cpp

    # GPU / CPU Monitor (Session 10)
    gpu_monitor/GPUMonitor.cpp
    gpu_monitor/CPUMonitor.cpp
)

target_include_directories(aurora_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core
        ${CMAKE_SOURCE_DIR}
)

target_compile_features(aurora_core PUBLIC cxx_std_20)

# Link FFmpeg
if(FFMPEG_FOUND)
    target_link_libraries(aurora_core PUBLIC PkgConfig::FFMPEG)
elseif(AVCODEC_LIB)
    target_link_libraries(aurora_core PUBLIC
        ${AVCODEC_LIB} ${AVFORMAT_LIB}
        ${AVUTIL_LIB}  ${SWSCALE_LIB}
        ${SWRESAMPLE_LIB}
    )
endif()

# Link Vulkan
if(AURORA_USE_VULKAN AND Vulkan_FOUND)
    target_link_libraries(aurora_core PUBLIC Vulkan::Vulkan)
    target_compile_definitions(aurora_core PUBLIC AURORA_VULKAN=1)
endif()

# Link NCNN
if(ncnn_FOUND)
    target_link_libraries(aurora_core PUBLIC ncnn)
    target_compile_definitions(aurora_core PUBLIC AURORA_NCNN=1)
endif()

# Link ONNX Runtime
if(ONNXRUNTIME_LIB)
    target_link_libraries(aurora_core PUBLIC ${ONNXRUNTIME_LIB})
    target_include_directories(aurora_core PUBLIC ${ONNXRUNTIME_INCLUDE})
    target_compile_definitions(aurora_core PUBLIC AURORA_ONNX=1)
endif()

# Link libplacebo
if(LIBPLACEBO_FOUND)
    target_link_libraries(aurora_core PUBLIC PkgConfig::LIBPLACEBO)
    target_compile_definitions(aurora_core PUBLIC AURORA_LIBPLACEBO=1)
endif()

# ── FreeType (Session 8 — subtitle text rendering) ─────────────────────────────
# Optional: if not found, FreeTypeRenderer degrades gracefully to a
# semi-transparent placeholder box (see SubtitleRenderer fallback path).
find_package(Freetype QUIET)
if(Freetype_FOUND OR FREETYPE_FOUND)
    target_link_libraries(aurora_core PUBLIC Freetype::Freetype)
    target_compile_definitions(aurora_core PUBLIC AURORA_HAS_FREETYPE=1)
    message(STATUS "Aurora: FreeType found — subtitle text rendering enabled")
else()
    message(WARNING "Aurora: FreeType NOT found — subtitle text will render as placeholder boxes")
endif()


if(AURORA_USE_DX11)
    target_compile_definitions(aurora_core PUBLIC AURORA_DX11=1)
endif()

# DirectX 12 (Windows)
if(WIN32)
    target_link_libraries(aurora_core PUBLIC d3d12 dxgi d3dcompiler)
    target_compile_definitions(aurora_core PUBLIC AURORA_DX12=1)
    # WASAPI audio
    target_link_libraries(aurora_core PUBLIC ole32 avrt)
    # GPU Monitor — PDH performance counters (Session 10)
    target_link_libraries(aurora_core PUBLIC pdh)
endif()

# Android AAudio
if(ANDROID)
    target_link_libraries(aurora_core PUBLIC aaudio)
    target_compile_definitions(aurora_core PUBLIC AURORA_AAUDIO=1)
endif()

# Compile GLSL → SPIR-V
if(AURORA_USE_VULKAN)
    add_subdirectory(renderer/shaders)
    add_dependencies(aurora_core aurora_shaders)
endif()

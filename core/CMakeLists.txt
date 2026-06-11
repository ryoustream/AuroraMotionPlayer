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

    # Subtitle
    subtitle/SubtitleEngine.cpp
    subtitle/SubtitleParser.cpp
    subtitle/SubtitleRenderer.cpp

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

    # Interpolation
    interpolation/AuroraFlow.cpp
    interpolation/RIFEInterpolator.cpp
    interpolation/IFRNetInterpolator.cpp
    interpolation/FILMInterpolator.cpp
    interpolation/GMFlowInterpolator.cpp
    interpolation/MotionEstimator.cpp
    interpolation/MotionCompensator.cpp

    # Upscaler
    upscaler/RealESRGAN.cpp
    upscaler/SPAN.cpp
    upscaler/Anime4K.cpp
    upscaler/FSRCNN.cpp
    upscaler/UpscalerFactory.cpp

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


if(AURORA_USE_DX11)
    target_compile_definitions(aurora_core PUBLIC AURORA_DX11=1)
endif()

# Compile GLSL → SPIR-V
if(AURORA_USE_VULKAN)
    add_subdirectory(renderer/shaders)
    add_dependencies(aurora_core aurora_shaders)
endif()

# ╔══════════════════════════════════════════════════════════════════╗
# ║  Aurora Motion Player — Android JNI CMakeLists                  ║
# ║  Session 13 Fix: corrected include paths (6 levels to root)     ║
# ╚══════════════════════════════════════════════════════════════════╝
cmake_minimum_required(VERSION 3.25)
project(aurora_jni CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-specific flags
add_compile_options(-O2 -fvisibility=hidden -fPIC)

# ── ABI-specific optimisation flags ──────────────────────────────────────────
if(ANDROID_ABI STREQUAL "arm64-v8a")
    add_compile_options(-march=armv8-a+simd -mtune=cortex-a76)
elseif(ANDROID_ABI STREQUAL "x86_64")
    add_compile_options(-msse4.2 -maes -mpclmul)
endif()

# ── Compute project root (6 levels up from jni/ dir) ─────────────────────────
# Structure: <root>/mobile/android/app/src/main/jni/
#             6      5      4    3    2     1
get_filename_component(AURORA_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../"
    ABSOLUTE
)
message(STATUS "Aurora JNI: project root resolved to: ${AURORA_ROOT}")

# ── JNI bridge library ────────────────────────────────────────────────────────
add_library(aurora_jni SHARED
    AuroraJNI.cpp
    AuroraPlayerBridge.cpp
)

# Include core headers using the correct root path
target_include_directories(aurora_jni PRIVATE
    ${AURORA_ROOT}/core
    ${AURORA_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Android system libs
find_library(LOG_LIB     log)
find_library(ANDROID_LIB android)
find_library(MEDIANDK_LIB mediandk)
find_library(VULKAN_LIB  vulkan)
find_library(OPENGLES_LIB GLESv3)
find_library(EGL_LIB     EGL)
find_library(AAUDIO_LIB  aaudio)

target_link_libraries(aurora_jni PRIVATE
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${MEDIANDK_LIB}
    ${VULKAN_LIB}
    ${OPENGLES_LIB}
    ${EGL_LIB}
    ${AAUDIO_LIB}
)

# ── FFmpeg prebuilt (optional — skipped gracefully if absent) ─────────────────
set(FFMPEG_ABI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")
if(EXISTS "${FFMPEG_ABI_DIR}/lib/libavcodec.a")
    message(STATUS "Aurora JNI: FFmpeg prebuilt found for ${ANDROID_ABI}")
    target_link_libraries(aurora_jni PRIVATE
        "${FFMPEG_ABI_DIR}/lib/libavcodec.a"
        "${FFMPEG_ABI_DIR}/lib/libavformat.a"
        "${FFMPEG_ABI_DIR}/lib/libavutil.a"
        "${FFMPEG_ABI_DIR}/lib/libswscale.a"
        "${FFMPEG_ABI_DIR}/lib/libswresample.a"
    )
    target_include_directories(aurora_jni PRIVATE "${FFMPEG_ABI_DIR}/include")
    target_compile_definitions(aurora_jni PRIVATE AURORA_HAS_FFMPEG=1)
elseif(EXISTS "${FFMPEG_ABI_DIR}/libavcodec.a")
    # Legacy flat layout fallback
    message(STATUS "Aurora JNI: FFmpeg prebuilt (flat) found for ${ANDROID_ABI}")
    target_link_libraries(aurora_jni PRIVATE
        "${FFMPEG_ABI_DIR}/libavcodec.a"
        "${FFMPEG_ABI_DIR}/libavformat.a"
        "${FFMPEG_ABI_DIR}/libavutil.a"
        "${FFMPEG_ABI_DIR}/libswscale.a"
        "${FFMPEG_ABI_DIR}/libswresample.a"
    )
    target_compile_definitions(aurora_jni PRIVATE AURORA_HAS_FFMPEG=1)
else()
    message(STATUS "Aurora JNI: No FFmpeg prebuilt — stub mode (CI smoke build)")
    target_compile_definitions(aurora_jni PRIVATE AURORA_STUB_FFMPEG=1)
endif()

# ── NCNN prebuilt (optional — skipped gracefully if absent) ──────────────────
set(NCNN_ABI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ncnn/${ANDROID_ABI}")
if(EXISTS "${NCNN_ABI_DIR}/libncnn.a")
    message(STATUS "Aurora JNI: NCNN prebuilt found for ${ANDROID_ABI}")
    target_link_libraries(aurora_jni PRIVATE "${NCNN_ABI_DIR}/libncnn.a")
    target_include_directories(aurora_jni PRIVATE "${NCNN_ABI_DIR}/include")
    target_compile_definitions(aurora_jni PRIVATE AURORA_NCNN=1)
elseif(EXISTS "${NCNN_ABI_DIR}/lib/libncnn.a")
    message(STATUS "Aurora JNI: NCNN prebuilt (lib/) found for ${ANDROID_ABI}")
    target_link_libraries(aurora_jni PRIVATE "${NCNN_ABI_DIR}/lib/libncnn.a")
    target_include_directories(aurora_jni PRIVATE "${NCNN_ABI_DIR}/include")
    target_compile_definitions(aurora_jni PRIVATE AURORA_NCNN=1)
else()
    message(STATUS "Aurora JNI: No NCNN prebuilt — AI interpolation disabled (CI stub)")
    target_compile_definitions(aurora_jni PRIVATE AURORA_STUB_NCNN=1)
endif()

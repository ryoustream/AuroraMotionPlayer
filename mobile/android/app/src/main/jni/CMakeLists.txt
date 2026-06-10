cmake_minimum_required(VERSION 3.25)
project(aurora_jni CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android-specific flags
add_compile_options(-O2 -fvisibility=hidden -fPIC)

# ── JNI bridge library ────────────────────────────────────────────────────────
add_library(aurora_jni SHARED
    AuroraJNI.cpp
    AuroraPlayerBridge.cpp
)

# Include core headers
target_include_directories(aurora_jni PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../../core
    ${CMAKE_SOURCE_DIR}/../../../../
)

# Android system libs
find_library(LOG_LIB    log)
find_library(ANDROID_LIB android)
find_library(MEDIANDK_LIB mediandk)
find_library(VULKAN_LIB vulkan)
find_library(OPENGLES_LIB GLESv3)
find_library(EGL_LIB    EGL)
find_library(AAUDIO_LIB aaudio)

target_link_libraries(aurora_jni PRIVATE
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${MEDIANDK_LIB}
    ${VULKAN_LIB}
    ${OPENGLES_LIB}
    ${EGL_LIB}
    ${AAUDIO_LIB}
)

# If building with prebuilt FFmpeg for Android
if(EXISTS "${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavcodec.a")
    target_link_libraries(aurora_jni PRIVATE
        ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavcodec.a
        ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavformat.a
        ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavutil.a
        ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libswscale.a
        ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libswresample.a
    )
endif()

# NCNN Android prebuilt
if(EXISTS "${CMAKE_SOURCE_DIR}/ncnn/${ANDROID_ABI}/libncnn.a")
    target_link_libraries(aurora_jni PRIVATE
        ${CMAKE_SOURCE_DIR}/ncnn/${ANDROID_ABI}/libncnn.a)
    target_compile_definitions(aurora_jni PRIVATE AURORA_NCNN=1)
endif()

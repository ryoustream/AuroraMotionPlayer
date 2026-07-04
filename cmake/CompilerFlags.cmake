# cmake/CompilerFlags.cmake
# Compiler-specific flags for Aurora Motion Player

if(MSVC)
    add_compile_options(
        /W3              # Warnings (was /W4 — too strict with permissive-)
        /WX-             # Warnings not errors
        /Zc:__cplusplus  # Correct __cplusplus value
        /utf-8           # UTF-8 source/exec charset
        /MP              # Multi-processor compilation
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        # /permissive- removed — causes C++20 string ctor overload failures in MSVC 14.44+
    )
    add_compile_definitions(
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
    )
endif()

# SIMD support
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
    if(MSVC)
        add_compile_options(/arch:AVX2)
    else()
        add_compile_options(-mavx2 -mfma)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    if(NOT MSVC)
        add_compile_options(-march=armv8-a+simd)
    endif()
endif()

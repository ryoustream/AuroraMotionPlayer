# cmake/CompilerFlags.cmake
# Compiler-specific flags for Aurora Motion Player

if(MSVC)
    add_compile_options(
        /W4
        /WX-
        /permissive-
        /Zc:__cplusplus
        /utf-8
        /MP              # Multi-processor compilation
        /O2              # Optimization for release
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
    )
    add_compile_definitions(
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
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

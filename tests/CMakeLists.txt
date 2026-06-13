cmake_minimum_required(VERSION 3.25)

# ── Test dependencies ─────────────────────────────────────────────────────────
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()
include(GoogleTest)

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(aurora_tests
    unit/test_VideoFrame.cpp
    unit/test_AudioBuffer.cpp
    unit/test_SubtitleParser.cpp
    unit/test_SceneDetector.cpp
    unit/test_ToneMapper.cpp
    unit/test_MotionEstimator.cpp
    unit/test_BenchmarkSystem.cpp
    unit/test_NetworkSource.cpp
    # Session 3
    unit/test_PluginManager.cpp
    unit/test_AIPipeline.cpp
    unit/test_AudioResampler.cpp
    # Session 4
    unit/test_InterpolationPipeline.cpp
    # Session 5
    unit/test_Upscaler.cpp
)

target_include_directories(aurora_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(aurora_tests PRIVATE
    aurora_core
    GTest::gtest_main
    GTest::gmock
)

target_compile_features(aurora_tests PRIVATE cxx_std_20)

gtest_discover_tests(aurora_tests
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

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

# ══════════════════════════════════════════════════════════════════════════════
# Unit Test Executable (S1–S10)
# ══════════════════════════════════════════════════════════════════════════════
add_executable(aurora_tests
    # S1-S2
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
    # Session 8
    unit/test_PGSParser.cpp
    unit/test_ASSRenderer.cpp
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

# ══════════════════════════════════════════════════════════════════════════════
# Integration Test Executable (S11)
# ══════════════════════════════════════════════════════════════════════════════
option(AURORA_BUILD_INTEGRATION_TESTS "Build integration tests (S11)" ON)

if(AURORA_BUILD_INTEGRATION_TESTS)
    add_executable(aurora_integration_tests
        # Session 11 — Integration Tests
        integration/test_Decoder.cpp
        integration/test_Pipeline.cpp
        integration/test_Subtitle.cpp
        integration/test_Network.cpp
    )

    target_include_directories(aurora_integration_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/core
        ${CMAKE_SOURCE_DIR}
    )

    target_link_libraries(aurora_integration_tests PRIVATE
        aurora_core
        GTest::gtest_main
        GTest::gmock
    )

    target_compile_features(aurora_integration_tests PRIVATE cxx_std_20)

    # Pass fixture directory as compile definition so tests can locate them
    target_compile_definitions(aurora_integration_tests PRIVATE
        AURORA_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
    )

    # Integration tests use a longer timeout (may do file I/O, subprocess)
    gtest_discover_tests(aurora_integration_tests
        TIMEOUT 120
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TEST_PREFIX "Integration."
    )

    message(STATUS "Aurora: Integration tests ENABLED")
else()
    message(STATUS "Aurora: Integration tests DISABLED (AURORA_BUILD_INTEGRATION_TESTS=OFF)")
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Benchmark Smoke Test (standalone binary, S10)
# ══════════════════════════════════════════════════════════════════════════════
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/test_benchmark_smoke.cpp)
    add_executable(aurora_benchmark_smoke
        benchmark/test_benchmark_smoke.cpp
    )
    target_include_directories(aurora_benchmark_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/core
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(aurora_benchmark_smoke PRIVATE
        aurora_core
    )
    target_compile_features(aurora_benchmark_smoke PRIVATE cxx_std_20)
endif()

cmake_minimum_required(VERSION 3.22)

project(AuroraMotionPlayer
    VERSION 1.0.0
    DESCRIPTION "Aurora Motion Player - AI-powered video player"
    LANGUAGES CXX C
)

# ── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build Options ────────────────────────────────────────────────────────────
option(AURORA_BUILD_DESKTOP  "Build desktop (Windows/Linux) target"  ON)
option(AURORA_BUILD_TESTS    "Build unit tests"                       ON)
option(AURORA_USE_VULKAN     "Enable Vulkan renderer"                 ON)
option(AURORA_USE_DX12       "Enable DirectX 12 renderer"             OFF)
option(AURORA_USE_DX11       "Enable DirectX 11 renderer"             OFF)
option(AURORA_USE_CUDA       "Enable CUDA GPU acceleration"           OFF)
option(AURORA_USE_TENSORRT   "Enable TensorRT inference"              OFF)
option(AURORA_USE_OPENVINO   "Enable OpenVINO inference"              OFF)

# ── Output Directories ───────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── CMake Modules ────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(cmake/Dependencies.cmake)
include(cmake/CompilerFlags.cmake)

# ── Sub-projects ─────────────────────────────────────────────────────────────
add_subdirectory(core)

if(AURORA_BUILD_DESKTOP)
    add_subdirectory(desktop/windows)
endif()

if(AURORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Installer (Windows only, optional) ───────────────────────────────────────
if(WIN32 AND AURORA_BUILD_DESKTOP)
    add_subdirectory(installer)
endif()

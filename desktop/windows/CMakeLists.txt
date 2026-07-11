cmake_minimum_required(VERSION 3.22)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Multimedia Network Concurrent Svg OpenGL
)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.3.0")
endif()

set(AURORA_SOURCES
    # Core UI
    src/main.cpp
    src/MainWindow.cpp          src/MainWindow.h
    src/PlayerWidget.cpp        src/PlayerWidget.h
    src/PlaylistWidget.cpp      src/PlaylistWidget.h
    src/ControlBar.cpp          src/ControlBar.h
    src/SettingsDialog.cpp      src/SettingsDialog.h
    src/AISettingsPanel.cpp     src/AISettingsPanel.h
    src/BenchmarkOverlay.cpp    src/BenchmarkOverlay.h
    src/SubtitleOverlay.cpp     src/SubtitleOverlay.h
    src/AuroraApplication.cpp   src/AuroraApplication.h

    # Session 6 — Windows UI Extensions
    src/ThumbnailBar.cpp        src/ThumbnailBar.h
    src/ChapterWidget.cpp       src/ChapterWidget.h
    src/BookmarkManager.cpp     src/BookmarkManager.h
    src/PlaybackHistory.cpp     src/PlaybackHistory.h
    src/SpeedControl.cpp        src/SpeedControl.h
    src/FileAssociation.cpp     src/FileAssociation.h
    src/MiniModeWindow.cpp      src/MiniModeWindow.h
)

# Windows application
qt_add_executable(AuroraPlayer WIN32
    ${AURORA_SOURCES}
    resources/aurora.rc
)

target_include_directories(AuroraPlayer PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(AuroraPlayer PRIVATE
    aurora_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
    Qt6::Concurrent
    Qt6::Svg
    Qt6::OpenGL
)

# Windows-specific system libraries
if(WIN32)
    target_link_libraries(AuroraPlayer PRIVATE
        dwmapi
        winmm
        shlwapi
        shell32
        ole32
        user32
        gdi32
        advapi32   # RegCreateKeyEx etc. used by FileAssociation
    )
    if(AURORA_USE_DX12 AND D3D12_LIB)
        target_link_libraries(AuroraPlayer PRIVATE ${D3D12_LIB} ${DXGI_LIB})
    endif()
    if(AURORA_USE_DX11 AND D3D11_LIB)
        target_link_libraries(AuroraPlayer PRIVATE ${D3D11_LIB})
    endif()
endif()

set_target_properties(AuroraPlayer PROPERTIES
    OUTPUT_NAME    "AuroraPlayer"
    WIN32_EXECUTABLE ON
)

# Deploy Qt DLLs alongside executable (Windows Release builds)
if(WIN32 AND Qt6_FOUND AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET AuroraPlayer POST_BUILD
        COMMAND ${Qt6_DIR}/../../../bin/windeployqt
            --release
            --no-translations
            --no-system-d3d-compiler
            "$<TARGET_FILE:AuroraPlayer>"
        COMMENT "Running windeployqt..."
    )
endif()

install(TARGETS AuroraPlayer DESTINATION bin)

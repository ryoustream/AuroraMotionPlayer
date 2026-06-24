cmake_minimum_required(VERSION 3.24)

# ── Installer / packaging targets ────────────────────────────────────────────
# These targets are available on Windows only and depend on WiX 4 being present.
# They are not built as part of the default ALL target.

if(NOT WIN32)
  return()
endif()

# ── Locate WiX 4 ─────────────────────────────────────────────────────────────
find_program(WIX_EXECUTABLE wix
  HINTS
    "$ENV{LOCALAPPDATA}/Programs/WiX Toolset v4/bin"
    "C:/Program Files (x86)/WiX Toolset v4/bin"
    "C:/Program Files/WiX Toolset v4/bin"
    "$ENV{PATH}"
)

if(NOT WIX_EXECUTABLE)
  message(STATUS "[Installer] WiX 4 not found — installer targets unavailable.")
  message(STATUS "[Installer] Install via: dotnet tool install --global wix")
  return()
endif()

message(STATUS "[Installer] WiX 4 found: ${WIX_EXECUTABLE}")

# ── Version ───────────────────────────────────────────────────────────────────
if(NOT DEFINED AURORA_VERSION)
  file(READ "${CMAKE_SOURCE_DIR}/VERSION" AURORA_VERSION_RAW)
  string(STRIP "${AURORA_VERSION_RAW}" AURORA_VERSION)
endif()

# Pad to 4 parts
string(REPLACE "." ";" _VER_PARTS "${AURORA_VERSION}")
list(LENGTH _VER_PARTS _VER_LEN)
while(_VER_LEN LESS 4)
  list(APPEND _VER_PARTS "0")
  list(LENGTH _VER_PARTS _VER_LEN)
endwhile()
list(JOIN _VER_PARTS "." WIX_VERSION)

message(STATUS "[Installer] Version: ${AURORA_VERSION} → WiX: ${WIX_VERSION}")

# ── Paths ─────────────────────────────────────────────────────────────────────
set(INSTALLER_DIR     "${CMAKE_CURRENT_SOURCE_DIR}")
set(WIX_DIR           "${INSTALLER_DIR}/wix")
set(INSTALLER_OUT_DIR "${CMAKE_BINARY_DIR}/installer")
set(BUILD_RELEASE_DIR "$<TARGET_FILE_DIR:AuroraPlayer>")

file(MAKE_DIRECTORY "${INSTALLER_OUT_DIR}")

# ── MSI target ────────────────────────────────────────────────────────────────
add_custom_target(aurora_msi
  COMMAND "${WIX_EXECUTABLE}"
    build
    -arch x64
    -o "${INSTALLER_OUT_DIR}/AuroraPlayer.msi"
    -d "BundleVersion=${WIX_VERSION}"
    -d "BuildDir=${BUILD_RELEASE_DIR}"
    "${WIX_DIR}/aurora.wxs"
    "${WIX_DIR}/aurora-ui.wxs"
  DEPENDS AuroraPlayer aurora_core
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "[Installer] Building AuroraPlayer.msi (WiX 4)"
  VERBATIM
)

# ── Bundle EXE target ─────────────────────────────────────────────────────────
add_custom_target(aurora_setup
  COMMAND "${WIX_EXECUTABLE}"
    build
    -arch x64
    -o "${INSTALLER_OUT_DIR}/AuroraPlayerSetup-x64.exe"
    -d "BundleVersion=${WIX_VERSION}"
    "${WIX_DIR}/aurora-bundle.wxs"
  DEPENDS aurora_msi
  WORKING_DIRECTORY "${INSTALLER_OUT_DIR}"
  COMMENT "[Installer] Building AuroraPlayerSetup-x64.exe (Burn bootstrapper)"
  VERBATIM
)

# ── Checksums target ──────────────────────────────────────────────────────────
add_custom_target(aurora_installer_checksums
  COMMAND ${CMAKE_COMMAND} -E echo "Computing SHA-256..."
  COMMAND PowerShell -NoProfile -Command
    "Get-ChildItem '${INSTALLER_OUT_DIR}' -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } | ForEach-Object { $h = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(); \"$h  $($_.Name)\" } | Set-Content '${INSTALLER_OUT_DIR}/SHA256SUMS.txt'"
  DEPENDS aurora_setup
  COMMENT "[Installer] Generating SHA256SUMS.txt"
  VERBATIM
)

# ── Convenience: all installer artefacts ─────────────────────────────────────
add_custom_target(aurora_installer_all
  DEPENDS aurora_msi aurora_setup aurora_installer_checksums
  COMMENT "[Installer] All installer artefacts built in: ${INSTALLER_OUT_DIR}"
)

# ── Install target (CPack alternative) ───────────────────────────────────────
install(
  FILES
    "${INSTALLER_OUT_DIR}/AuroraPlayer.msi"
    "${INSTALLER_OUT_DIR}/AuroraPlayerSetup-x64.exe"
    "${INSTALLER_OUT_DIR}/SHA256SUMS.txt"
  DESTINATION "."
  OPTIONAL
)

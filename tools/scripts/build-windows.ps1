#!/usr/bin/env pwsh
# Aurora Motion Player - Windows Build Script
# Usage: .\tools\scripts\build-windows.ps1 [-BuildType Release|Debug] [-RunTests] [-Package]

param(
    [string]$BuildType   = "Release",
    [switch]$RunTests    = $false,
    [switch]$Package     = $false,
    [switch]$Clean       = $false,
    [int]   $Jobs        = $env:NUMBER_OF_PROCESSORS
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root     = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$BuildDir = "$Root\build"

function Log([string]$msg) { Write-Host "[Aurora] $msg" -ForegroundColor Cyan }
function Die([string]$msg) { Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

Log "Build type: $BuildType | Jobs: $Jobs"

# Clean
if ($Clean -and (Test-Path $BuildDir)) {
    Log "Cleaning build directory..."
    Remove-Item -Recurse -Force $BuildDir
}

# Check prerequisites
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Die "CMake not found" }

# vcpkg
$VcpkgRoot = "$Root\vcpkg"
if (-not (Test-Path $VcpkgRoot)) {
    Log "Bootstrapping vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
}

# Configure
Log "Configuring CMake..."
$configArgs = @(
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022", "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "-DAURORA_BUILD_DESKTOP=ON",
    "-DAURORA_BUILD_TESTS=ON",
    "-DAURORA_USE_VULKAN=ON",
    "-DAURORA_USE_DX11=ON"
)

if ($env:Qt6_DIR)      { $configArgs += "-DQt6_DIR=$env:Qt6_DIR" }
if ($env:VULKAN_SDK)   { $configArgs += "-DVULKAN_SDK=$env:VULKAN_SDK" }

cmake $Root @configArgs
if ($LASTEXITCODE -ne 0) { Die "CMake configuration failed" }

# Build
Log "Building ($Jobs parallel jobs)..."
cmake --build $BuildDir --config $BuildType --parallel $Jobs -- /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { Die "Build failed" }

Log "Build succeeded!"

# Tests
if ($RunTests) {
    Log "Running unit tests..."
    Push-Location $BuildDir
    ctest -C $BuildType --output-on-failure --timeout 60 -j4
    if ($LASTEXITCODE -ne 0) { Die "Tests failed" }
    Pop-Location
    Log "All tests passed!"
}

# Package
if ($Package -and $BuildType -eq "Release") {
    Log "Packaging..."
    $exe = "$BuildDir\bin\$BuildType\AuroraPlayer.exe"
    if (Test-Path $exe) {
        $pkg = "$Root\dist\AuroraPlayer-1.0.0-win64"
        New-Item -ItemType Directory -Force $pkg | Out-Null
        Copy-Item "$BuildDir\bin\$BuildType\*" $pkg -Recurse -Force
        Compress-Archive -Path $pkg -DestinationPath "$pkg.zip" -Force
        Log "Package: $pkg.zip"
    } else {
        Log "Warning: AuroraPlayer.exe not found, skipping packaging"
    }
}

Log "Done."

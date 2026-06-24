# Aurora Motion Player — Windows Installer

## Overview

The Windows installer for Aurora Motion Player is built using **WiX Toolset v4** and produces:

| Artefact | Description |
|---|---|
| `AuroraPlayer.msi` | Core MSI package (per-machine, x64) |
| `AuroraPlayerSetup-x64.exe` | Burn bootstrapper — chains VC++ Redist + MSI |
| `SHA256SUMS.txt` | SHA-256 checksums for verification |

---

## Building the Installer

### Prerequisites

- **Windows 10/11 x64**
- **CMake + Ninja** — to build the C++ core
- **WiX Toolset v4** — install via dotnet global tool:
  ```powershell
  dotnet tool install --global wix --version 4.0.5
  ```
- **Python 3.8+** — for the model downloader script
- *(Optional)* Windows SDK `signtool.exe` for code signing

### Quick build

```powershell
# 1. Build C++ core first
cmake -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows --config Release

# 2. Build installer
.\tools\scripts\build-installer.ps1 -Version 1.0.0
```

Output lands in `dist/installer/`.

### Build with signing

```powershell
$env:SIGN_THUMBPRINT = "ABCDEF1234..."  # your certificate thumbprint
.\tools\scripts\build-installer.ps1 -Version 1.0.0 -Sign
```

### Options

| Parameter | Default | Description |
|---|---|---|
| `-Version` | reads `VERSION` | Semver string, e.g. `1.2.0` |
| `-BuildDir` | `build\windows\Release` | CMake Release output dir |
| `-OutDir` | `dist\installer` | Where MSI/EXE land |
| `-Sign` | off | Code-sign with `SIGN_THUMBPRINT` |
| `-SkipBundle` | off | Build MSI only, skip Setup EXE |

---

## WiX Source Files

| File | Purpose |
|---|---|
| `wix/aurora.wxs` | Main product definition, features, components, registry |
| `wix/aurora-ui.wxs` | Custom WiX UI dialog flow and branding |
| `wix/aurora-bundle.wxs` | Burn bootstrapper (chains VC++ Redist → MSI) |

---

## AI Model Download

After installation, download AI models via:

```powershell
# From Start Menu shortcut or manually:
AuroraPlayer.exe --download-models

# Or via PowerShell script:
.\tools\scripts\download-models.ps1 -Pack standard

# Or Python directly:
python tools\models\download_models.py --pack standard --list
```

### Pack sizes

| Pack | Includes | Approx. size |
|---|---|---|
| `lite` | RIFE v4.6, RealESRGAN Anime, Anime4K, FSRCNN | ~100 MB |
| `standard` | + RIFE v4.18, RealESRGAN x4+, SPAN, IFRNet-S, GMFlow | ~300 MB |
| `full` | + IFRNet-L, FILM | ~600 MB |

---

## CI/CD

The installer is built automatically on every push to `main` and on every tag via:

```
.github/workflows/build-installer.yml
```

On tag pushes (`v*.*.*`), the MSI and Setup EXE are attached to the GitHub Release
alongside the Windows ZIP and Android APK.

---

## Install Options (silent)

```bat
# Silent install, default directory
msiexec /i AuroraPlayer.msi /quiet /norestart

# Silent install, custom directory
msiexec /i AuroraPlayer.msi /quiet INSTALLFOLDER="D:\Apps\AuroraPlayer"

# Uninstall silently
msiexec /x AuroraPlayer.msi /quiet /norestart

# Via bootstrapper EXE
AuroraPlayerSetup-x64.exe /silent /install
AuroraPlayerSetup-x64.exe /silent /uninstall
```

---

## File Associations

The installer registers Aurora Motion Player as an option for:

`MKV · MP4 · AVI · MOV · WEBM · M2TS · TS`

These are registered as `OpenWithProgids` entries, so they appear in
**Open With** without overriding the user's current default player.

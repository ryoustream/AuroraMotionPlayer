#Requires -Version 5.1
<#
.SYNOPSIS
    Aurora Motion Player — AI Model Downloader (Windows wrapper)
    Calls the Python download_models.py script, installing Python if needed.

.PARAMETER Pack
    lite | standard | full   (default: lite)

.PARAMETER Model
    Filter by family: rife, realesrgan, span, anime4k, fsrcnn, ifrnet, film, gmflow
    Or group aliases:  interpolation, upscaling

.PARAMETER OutDir
    Override model output directory (default: .\models\ next to player)

.PARAMETER List
    List available models and exit.

.PARAMETER SkipVerify
    Skip SHA-256 hash verification.

.PARAMETER DryRun
    Show what would be downloaded without downloading.

.EXAMPLE
    .\download-models.ps1 -Pack standard
    .\download-models.ps1 -Model rife -Pack full
#>

[CmdletBinding()]
param(
    [ValidateSet("lite","standard","full")]
    [string]$Pack      = "lite",
    [string]$Model     = "",
    [string]$OutDir    = "",
    [switch]$List,
    [switch]$SkipVerify,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$banner = @"
╔══════════════════════════════════════════════════════════╗
║     Aurora Motion Player — AI Model Downloader           ║
╚══════════════════════════════════════════════════════════╝
"@
Write-Host $banner -ForegroundColor Cyan

# ─── Locate Python ────────────────────────────────────────────────────────────
function Find-Python {
    foreach ($cmd in @("python", "python3", "py")) {
        $p = Get-Command $cmd -ErrorAction SilentlyContinue
        if ($p) {
            $ver = (& $p.Source --version 2>&1).ToString()
            if ($ver -match "Python 3\.[89]|Python 3\.1[0-9]") {
                return $p.Source
            }
        }
    }
    return $null
}

$python = Find-Python
if (-not $python) {
    Write-Host "Python 3.8+ not found. Attempting to install via winget..." -ForegroundColor Yellow
    try {
        winget install --id Python.Python.3.11 --silent --accept-source-agreements --accept-package-agreements
        $python = Find-Python
    } catch {
        Write-Host "winget install failed. Please install Python 3.8+ from https://python.org" -ForegroundColor Red
        Write-Host "Then re-run this script." -ForegroundColor Red
        exit 1
    }
}

if (-not $python) {
    Write-Host "Python 3.8+ is required. Download from: https://python.org" -ForegroundColor Red
    exit 1
}

Write-Host "Python: $python" -ForegroundColor Green

# ─── Locate download_models.py ────────────────────────────────────────────────
$scriptRoot = $PSScriptRoot
$pyScript   = Join-Path $scriptRoot "..\models\download_models.py"

if (-not (Test-Path $pyScript)) {
    # Try relative to player EXE
    $exeDir  = Split-Path (Get-Process -Id $PID).Path -Parent
    $pyScript = Join-Path $exeDir "..\tools\models\download_models.py"
}

if (-not (Test-Path $pyScript)) {
    Write-Host "download_models.py not found at: $pyScript" -ForegroundColor Red
    exit 1
}

$pyScript = Resolve-Path $pyScript

# ─── Build argument list ──────────────────────────────────────────────────────
$pyArgs = @("$pyScript", "--pack", $Pack)

if ($Model)      { $pyArgs += @("--model",   $Model)  }
if ($OutDir)     { $pyArgs += @("--out-dir",  $OutDir) }
if ($List)       { $pyArgs += "--list"                 }
if ($SkipVerify) { $pyArgs += "--skip-verify"          }
if ($DryRun)     { $pyArgs += "--dry-run"              }

# ─── Run ──────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Running: python $($pyArgs -join ' ')" -ForegroundColor DarkGray
Write-Host ""

& $python @pyArgs
exit $LASTEXITCODE

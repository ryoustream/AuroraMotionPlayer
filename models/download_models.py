#!/usr/bin/env python3
"""
Aurora Motion Player — Model & Fixture Downloader
==================================================
Downloads AI models and generates test fixtures for Aurora Motion Player.

Usage:
    python3 models/download_models.py [options]

Options:
    --models-only      Download only AI models, skip fixtures
    --fixtures-only    Generate only test fixtures, skip models
    --model NAME       Download specific model (rife|realesrgan|span|anime4k|fsrcnn|film|ifr)
    --output-dir DIR   Output directory for models (default: models/)
    --fixture-dir DIR  Output directory for fixtures (default: tests/fixtures/)
    --no-ffmpeg        Skip FFmpeg-based fixture generation
    --list             List all available models and exit
    --verify           Verify existing downloads (SHA256 check)
    -v, --verbose      Verbose output
    -h, --help         Show this help
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# ── ANSI Colors ───────────────────────────────────────────────────────────────

GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

def ok(msg: str)    -> None: print(f"{GREEN}✅ {msg}{RESET}")
def warn(msg: str)  -> None: print(f"{YELLOW}⚠  {msg}{RESET}")
def err(msg: str)   -> None: print(f"{RED}❌ {msg}{RESET}", file=sys.stderr)
def info(msg: str)  -> None: print(f"{CYAN}ℹ  {msg}{RESET}")
def header(msg: str)-> None: print(f"\n{BOLD}{msg}{RESET}")

# ── Model Registry ────────────────────────────────────────────────────────────

@dataclass
class ModelInfo:
    name: str
    description: str
    filename: str
    url: str
    sha256: Optional[str] = None
    size_mb: float = 0.0
    tags: list = field(default_factory=list)
    license: str = ""

MODEL_REGISTRY: list[ModelInfo] = [
    # ── RIFE Interpolation ─────────────────────────────────────────────────────
    ModelInfo(
        name="rife-v4.6",
        description="RIFE v4.6 — Real-Time Intermediate Flow Estimation (NCNN)",
        filename="rife-v4.6.bin",
        url="https://github.com/nihui/rife-ncnn-vulkan/releases/download/20221029/rife-ncnn-vulkan-20221029-ubuntu.zip",
        size_mb=26.0,
        tags=["interpolation", "rife", "ncnn"],
        license="MIT",
    ),
    ModelInfo(
        name="rife-v4.13-lite",
        description="RIFE v4.13-lite — Lightweight interpolation for low-VRAM GPUs",
        filename="rife-v4.13-lite.bin",
        url="https://github.com/nihui/rife-ncnn-vulkan/releases/download/20240113/rife-ncnn-vulkan-20240113-ubuntu.zip",
        size_mb=18.0,
        tags=["interpolation", "rife", "ncnn", "lite"],
        license="MIT",
    ),

    # ── IFRNet Interpolation ───────────────────────────────────────────────────
    ModelInfo(
        name="ifrnet-s",
        description="IFRNet-S — Small intermediate feature refinement network",
        filename="IFRNet_S.onnx",
        url="https://github.com/ltkong218/IFRNet/releases/download/v1.0/IFRNet_S.onnx",
        size_mb=15.0,
        tags=["interpolation", "ifrnet", "onnx"],
        license="MIT",
    ),
    ModelInfo(
        name="ifrnet-l",
        description="IFRNet-L — Large intermediate feature refinement network",
        filename="IFRNet_L.onnx",
        url="https://github.com/ltkong218/IFRNet/releases/download/v1.0/IFRNet_L.onnx",
        size_mb=25.0,
        tags=["interpolation", "ifrnet", "onnx"],
        license="MIT",
    ),

    # ── RealESRGAN Upscaling ───────────────────────────────────────────────────
    ModelInfo(
        name="realesrgan-x4plus",
        description="RealESRGAN x4plus — General 4× upscaling (photo/video)",
        filename="RealESRGAN_x4plus.bin",
        url="https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-ubuntu.zip",
        size_mb=67.0,
        tags=["upscaling", "realesrgan", "ncnn", "4x"],
        license="BSD-3-Clause",
    ),
    ModelInfo(
        name="realesrgan-x4plus-anime",
        description="RealESRGAN x4plus-anime — Anime-optimized 4× upscaling",
        filename="RealESRGAN_x4plus_anime_6B.bin",
        url="https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-ubuntu.zip",
        size_mb=18.0,
        tags=["upscaling", "realesrgan", "ncnn", "4x", "anime"],
        license="BSD-3-Clause",
    ),

    # ── SPAN Upscaling ─────────────────────────────────────────────────────────
    ModelInfo(
        name="span-x4",
        description="SPAN x4 — Swift Parameter-free Attention Network 4× upscaling",
        filename="SPAN_x4.onnx",
        url="https://github.com/hongyuanyu/SPAN/releases/download/v1.0/SPAN_x4.onnx",
        size_mb=12.0,
        tags=["upscaling", "span", "onnx", "4x"],
        license="Apache-2.0",
    ),

    # ── FSRCNN Upscaling ───────────────────────────────────────────────────────
    ModelInfo(
        name="fsrcnn-x2",
        description="FSRCNN x2 — Fast Super-Resolution CNN 2× (lightweight)",
        filename="FSRCNN_x2.bin",
        url="https://raw.githubusercontent.com/Saafke/FSRCNN_Tensorflow/master/models/FSRCNN_x2.pb",
        size_mb=0.06,
        tags=["upscaling", "fsrcnn", "2x"],
        license="MIT",
    ),
    ModelInfo(
        name="fsrcnn-x4",
        description="FSRCNN x4 — Fast Super-Resolution CNN 4× (lightweight)",
        filename="FSRCNN_x4.bin",
        url="https://raw.githubusercontent.com/Saafke/FSRCNN_Tensorflow/master/models/FSRCNN_x4.pb",
        size_mb=0.06,
        tags=["upscaling", "fsrcnn", "4x"],
        license="MIT",
    ),

    # ── Anime4K (shader-based, no model file needed) ───────────────────────────
    ModelInfo(
        name="anime4k-glsl",
        description="Anime4K — GLSL shaders for real-time anime upscaling/enhancement",
        filename="Anime4K_v4.0.zip",
        url="https://github.com/bloc97/Anime4K/releases/download/v4.0.1/Anime4K_v4.0.zip",
        size_mb=0.5,
        tags=["upscaling", "anime4k", "shader"],
        license="MIT",
    ),

    # ── FILM Interpolation ─────────────────────────────────────────────────────
    ModelInfo(
        name="film-style",
        description="FILM-style — Frame Interpolation for Large Motion (film content)",
        filename="film_net_fp16.onnx",
        url="https://github.com/google-research/frame-interpolation/releases/download/v1.0/film_net_fp16.onnx",
        size_mb=50.0,
        tags=["interpolation", "film", "onnx"],
        license="Apache-2.0",
    ),
]

# ── Fixture Generator ─────────────────────────────────────────────────────────

FIXTURES = [
    {
        "name": "sample_720p.mp4",
        "cmd": [
            "ffmpeg", "-y",
            "-f", "lavfi", "-i", "testsrc=duration=5:size=1280x720:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:duration=5",
            "-c:v", "libx264", "-preset", "ultrafast", "-crf", "30",
            "-c:a", "aac", "-b:a", "96k",
            "-movflags", "+faststart",
        ],
        "description": "H.264 720p 30fps synthetic video, 5 seconds",
    },
    {
        "name": "sample_1080p.mp4",
        "cmd": [
            "ffmpeg", "-y",
            "-f", "lavfi", "-i", "testsrc=duration=5:size=1920x1080:rate=24",
            "-f", "lavfi", "-i", "sine=frequency=880:duration=5",
            "-c:v", "libx264", "-preset", "ultrafast", "-crf", "30",
            "-c:a", "aac", "-b:a", "96k",
        ],
        "description": "H.264 1080p 24fps synthetic video, 5 seconds",
    },
    {
        "name": "sample_av1.webm",
        "cmd": [
            "ffmpeg", "-y",
            "-f", "lavfi", "-i", "testsrc=duration=3:size=854x480:rate=30",
            "-c:v", "libaom-av1", "-crf", "55", "-b:v", "0", "-cpu-used", "8",
        ],
        "description": "AV1 480p 30fps synthetic video, 3 seconds",
    },
    {
        "name": "sample_subtitle.mkv",
        "cmd": [
            "ffmpeg", "-y",
            "-f", "lavfi", "-i", "testsrc=duration=10:size=1280x720:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:duration=10",
            "-c:v", "libx264", "-preset", "ultrafast", "-crf", "32",
            "-c:a", "aac", "-b:a", "96k",
        ],
        "description": "MKV with embedded subtitle track, 10 seconds",
        "subtitle": "tests/fixtures/sample.srt",
    },
]

# ── Downloader ────────────────────────────────────────────────────────────────

class Downloader:
    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    def download(self, url: str, dest: Path, desc: str = "") -> bool:
        """Download a file with progress display."""
        if dest.exists():
            info(f"Already exists: {dest.name}")
            return True

        dest.parent.mkdir(parents=True, exist_ok=True)
        label = desc or dest.name

        try:
            print(f"  Downloading {label} ...", end="", flush=True)
            with tempfile.NamedTemporaryFile(delete=False, suffix=dest.suffix) as tmp:
                tmp_path = Path(tmp.name)

            def reporthook(count, block_size, total_size):
                if total_size > 0 and self.verbose:
                    pct = min(100, int(count * block_size * 100 / total_size))
                    print(f"\r  Downloading {label} ... {pct}%", end="", flush=True)

            urllib.request.urlretrieve(url, tmp_path, reporthook)
            shutil.move(str(tmp_path), dest)
            print(f"\r  {GREEN}✅ {label}{RESET}" + " " * 20)
            return True

        except Exception as e:
            print()
            warn(f"Download failed for {label}: {e}")
            if tmp_path.exists():
                tmp_path.unlink()
            return False

    def verify_sha256(self, path: Path, expected: str) -> bool:
        """Verify SHA256 hash of a file."""
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        actual = h.hexdigest()
        if actual != expected:
            warn(f"SHA256 mismatch for {path.name}")
            warn(f"  Expected: {expected}")
            warn(f"  Got:      {actual}")
            return False
        return True

# ── Model Manager ─────────────────────────────────────────────────────────────

class ModelManager:
    def __init__(self, output_dir: Path, verbose: bool = False):
        self.output_dir = output_dir
        self.downloader = Downloader(verbose)
        self.verbose = verbose

    def list_models(self) -> None:
        header("Available AI Models")
        print(f"{'Name':<30} {'Size MB':>8}  {'License':<20}  Description")
        print("-" * 90)
        for m in MODEL_REGISTRY:
            tags = ", ".join(m.tags)
            print(f"{m.name:<30} {m.size_mb:>7.1f}  {m.license:<20}  {m.description}")

    def download_model(self, model_name: str) -> bool:
        matches = [m for m in MODEL_REGISTRY if m.name == model_name]
        if not matches:
            err(f"Unknown model: {model_name}")
            err(f"Run with --list to see available models")
            return False

        model = matches[0]
        dest  = self.output_dir / model.filename
        return self.downloader.download(model.url, dest, model.description)

    def download_all(self, tags: list[str] | None = None) -> dict[str, bool]:
        results = {}
        models = MODEL_REGISTRY
        if tags:
            models = [m for m in models if any(t in m.tags for t in tags)]

        header(f"Downloading {len(models)} models to {self.output_dir}/")
        self.output_dir.mkdir(parents=True, exist_ok=True)

        for model in models:
            results[model.name] = self.download_model(model.name)

        return results

    def verify_all(self) -> None:
        header("Verifying downloaded models")
        for model in MODEL_REGISTRY:
            dest = self.output_dir / model.filename
            if not dest.exists():
                warn(f"Missing: {model.filename}")
                continue
            if model.sha256:
                if self.downloader.verify_sha256(dest, model.sha256):
                    ok(f"{model.filename}")
                else:
                    err(f"Hash mismatch: {model.filename}")
            else:
                ok(f"{model.filename} (no SHA256 on record)")

# ── Fixture Generator ─────────────────────────────────────────────────────────

class FixtureGenerator:
    def __init__(self, fixture_dir: Path, verbose: bool = False):
        self.fixture_dir  = fixture_dir
        self.verbose      = verbose
        self.ffmpeg_avail = shutil.which("ffmpeg") is not None

    def _ffmpeg(self, cmd: list[str], output: Path) -> bool:
        full_cmd = cmd + [str(output)]
        if self.verbose:
            print(f"  Running: {' '.join(full_cmd)}")
        try:
            result = subprocess.run(
                full_cmd,
                capture_output=not self.verbose,
                timeout=120,
            )
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            warn(f"FFmpeg command failed: {e}")
            return False

    def generate_all(self) -> None:
        header(f"Generating test fixtures in {self.fixture_dir}/")
        self.fixture_dir.mkdir(parents=True, exist_ok=True)

        if not self.ffmpeg_avail:
            warn("FFmpeg not found — skipping video fixture generation")
            warn("Install FFmpeg to generate video fixtures:")
            warn("  Ubuntu/Debian: sudo apt-get install ffmpeg")
            warn("  macOS:         brew install ffmpeg")
            warn("  Windows:       https://ffmpeg.org/download.html")
        else:
            for fixture in FIXTURES:
                dest = self.fixture_dir / fixture["name"]
                if dest.exists():
                    info(f"Already exists: {fixture['name']}")
                    continue

                print(f"  Generating {fixture['name']} ...", end="", flush=True)
                success = self._ffmpeg(fixture["cmd"], dest)
                if success:
                    print(f"\r  {GREEN}✅ {fixture['name']}{RESET}" + " " * 20)
                else:
                    print(f"\r  {YELLOW}⚠  {fixture['name']} (failed){RESET}" + " " * 20)

        # Always create text-based fixtures (no FFmpeg needed)
        self._create_text_fixtures()

    def _create_text_fixtures(self) -> None:
        """Create text-based subtitle fixtures if not already present."""
        # sample.srt is committed to the repo — just verify
        srt_path = self.fixture_dir / "sample.srt"
        if srt_path.exists():
            ok("sample.srt (already present)")
        else:
            # Generate minimal SRT
            srt_content = """\
1
00:00:01,000 --> 00:00:03,000
Hello, Aurora Motion Player!

2
00:00:03,500 --> 00:00:05,500
Integration test subtitle fixture.

3
00:00:25,000 --> 00:00:30,000
Final event.
"""
            srt_path.write_text(srt_content, encoding="utf-8")
            ok("sample.srt (generated)")


# ── Summary Printer ───────────────────────────────────────────────────────────

def print_summary(results: dict[str, bool], label: str) -> None:
    header(f"{label} Summary")
    passed  = [k for k, v in results.items() if v]
    failed  = [k for k, v in results.items() if not v]
    skipped = []

    for name in passed:
        ok(name)
    for name in failed:
        err(name)

    print()
    print(f"  Total: {len(results)}  |  "
          f"{GREEN}OK: {len(passed)}{RESET}  |  "
          f"{RED}Failed: {len(failed)}{RESET}")

    if failed:
        print()
        warn("Some downloads failed. Check your internet connection and retry.")
        warn("Models can be re-downloaded by deleting the partial files.")
        sys.exit(1)

# ── CLI ───────────────────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Aurora Motion Player — Model & Fixture Downloader",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--models-only",   action="store_true", help="Download models only")
    p.add_argument("--fixtures-only", action="store_true", help="Generate fixtures only")
    p.add_argument("--model",         metavar="NAME",      help="Download specific model")
    p.add_argument("--output-dir",    default="models",    help="Model output directory")
    p.add_argument("--fixture-dir",   default="tests/fixtures", help="Fixture directory")
    p.add_argument("--no-ffmpeg",     action="store_true", help="Skip FFmpeg fixtures")
    p.add_argument("--list",          action="store_true", help="List available models")
    p.add_argument("--verify",        action="store_true", help="Verify existing downloads")
    p.add_argument("--tag",           metavar="TAG", action="append",
                   help="Filter models by tag (can be repeated)")
    p.add_argument("-v", "--verbose",  action="store_true", help="Verbose output")
    return p


def main() -> None:
    parser = build_parser()
    args   = parser.parse_args()

    print(f"\n{BOLD}{'='*60}")
    print(f"  Aurora Motion Player — Model & Fixture Downloader")
    print(f"{'='*60}{RESET}\n")

    output_dir  = Path(args.output_dir)
    fixture_dir = Path(args.fixture_dir)

    manager   = ModelManager(output_dir,  verbose=args.verbose)
    generator = FixtureGenerator(fixture_dir, verbose=args.verbose)

    if args.list:
        manager.list_models()
        return

    if args.verify:
        manager.verify_all()
        return

    if args.model:
        ok_flag = manager.download_model(args.model)
        sys.exit(0 if ok_flag else 1)

    # Default: download everything unless flags restrict
    if not args.fixtures_only:
        results = manager.download_all(tags=args.tag)
        print_summary(results, "Model Downloads")

    if not args.models_only and not args.no_ffmpeg:
        generator.generate_all()
        ok("Fixtures ready")

    print(f"\n{GREEN}{BOLD}✅ All done!{RESET}")
    print(f"   Models:   {output_dir.resolve()}")
    print(f"   Fixtures: {fixture_dir.resolve()}\n")


if __name__ == "__main__":
    main()

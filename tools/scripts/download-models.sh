#!/usr/bin/env bash
# Aurora Motion Player — AI Model Downloader (Linux/macOS wrapper)
# Usage: ./download-models.sh [--pack lite|standard|full] [--model <family>] [OPTIONS]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_SCRIPT="$SCRIPT_DIR/../models/download_models.py"

# ── Locate Python 3.8+ ──────────────────────────────────────────────────────
find_python() {
    for cmd in python3 python python3.11 python3.10 python3.9 python3.8; do
        if command -v "$cmd" &>/dev/null; then
            ver=$("$cmd" --version 2>&1 | grep -oP '3\.\d+' | head -1)
            major=${ver%%.*}; minor=${ver##*.}
            if [[ "$major" == "3" && "$minor" -ge 8 ]]; then
                echo "$cmd"
                return
            fi
        fi
    done
    echo ""
}

PYTHON=$(find_python)
if [[ -z "$PYTHON" ]]; then
    echo "❌ Python 3.8+ required. Install via your package manager:" >&2
    echo "   Ubuntu/Debian: sudo apt install python3" >&2
    echo "   macOS:         brew install python@3.11" >&2
    exit 1
fi

if [[ ! -f "$PY_SCRIPT" ]]; then
    echo "❌ download_models.py not found at: $PY_SCRIPT" >&2
    exit 1
fi

echo "╔══════════════════════════════════════════════════════════╗"
echo "║     Aurora Motion Player — AI Model Downloader           ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "  Python: $PYTHON"
echo "  Script: $PY_SCRIPT"
echo ""

exec "$PYTHON" "$PY_SCRIPT" "$@"

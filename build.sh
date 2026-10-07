#!/usr/bin/env bash
# Builds MarkFar (x64) with llvm-mingw and, under WSL, installs it into the
# Far user profile. Usage: ./build.sh [noinstall]
# Compiler: x86_64-w64-mingw32-clang++ on PATH, or llvm-mingw in ~/.local/opt.
set -euo pipefail
cd "$(dirname "$0")"

CXX=x86_64-w64-mingw32-clang++
if ! command -v "$CXX" >/dev/null; then
  for d in "$HOME"/.local/opt/llvm-mingw-*/bin; do PATH="$d:$PATH"; done
fi
command -v "$CXX" >/dev/null || { echo "llvm-mingw not found (see docs/dev-install.md)"; exit 1; }

OUT=out/MarkFar
mkdir -p "$OUT"
"$CXX" -std=c++20 -O2 -Wall -Wextra -municode -DUNICODE -D_UNICODE -Isdk \
  -shared -static -s -o "$OUT/MarkFar.dll" src/*.cpp src/markfar.def
mkdir -p "$OUT/hrc" && cp hrc/markfar.hrc "$OUT/hrc/"
echo "Built $OUT/MarkFar.dll"

[[ "${1:-}" == noinstall ]] && exit 0
command -v powershell.exe >/dev/null || exit 0   # not WSL: build only

APPDATA_WIN=$(powershell.exe -NoProfile -Command '$env:APPDATA' | tr -d '\r')
DEST="$(wslpath "$APPDATA_WIN")/Far Manager/Profile/Plugins/MarkFar"
mkdir -p "$DEST"
cp -r "$OUT"/* "$DEST/" || { echo "Install failed: close Far Manager and run build.sh again."; exit 1; }
echo "Installed to $APPDATA_WIN\\Far Manager\\Profile\\Plugins\\MarkFar"

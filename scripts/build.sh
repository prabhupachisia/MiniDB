#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
DIST_EXE="$ROOT/dist/minidb"

cmake -S "$ROOT" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

if [[ ! -x "$DIST_EXE" ]]; then
  echo "Build completed, but dist/minidb was not found or is not executable." >&2
  exit 1
fi

echo "Build complete."
echo "Run MiniDB with:"
echo "  ./dist/minidb"

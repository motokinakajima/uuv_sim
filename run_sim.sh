#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_JSON="$ROOT_DIR/simulation_data.json"

echo "Configuring project..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"

echo "Building project..."
cmake --build "$BUILD_DIR"

if [[ -x "$BUILD_DIR/uuv_sim" ]]; then
  EXECUTABLE="$BUILD_DIR/uuv_sim"
elif [[ -x "$BUILD_DIR/Debug/uuv_sim" ]]; then
  EXECUTABLE="$BUILD_DIR/Debug/uuv_sim"
elif [[ -x "$BUILD_DIR/Debug/uuv_sim.exe" ]]; then
  EXECUTABLE="$BUILD_DIR/Debug/uuv_sim.exe"
else
  echo "Error: Could not find built executable in $BUILD_DIR"
  exit 1
fi

echo "Running simulation..."
cd "$ROOT_DIR"
"$EXECUTABLE"

if [[ -f "$OUTPUT_JSON" ]]; then
  echo "Done: $OUTPUT_JSON"
else
  echo "Warning: simulation ran, but $OUTPUT_JSON was not found."
  exit 1
fi

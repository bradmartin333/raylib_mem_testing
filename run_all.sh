#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHM_NAME="${1:-/raylib_fb_rgb565}"

cleanup() {
	local pids=("${renderer_pid:-}" "${sdl2_pid:-}" "${raylib_pid:-}" "${tkinter_pid:-}")
	for pid in "${pids[@]}"; do
		if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
			kill "$pid" 2>/dev/null || true
		fi
	done
}

trap cleanup EXIT INT TERM

echo "[build] renderer"
cmake -S "$ROOT_DIR/renderer" -B "$ROOT_DIR/renderer/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/renderer/build" -j

echo "[build] viewer/sdl2-viewer"
cmake -S "$ROOT_DIR/viewer/sdl2-viewer" -B "$ROOT_DIR/viewer/sdl2-viewer/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/viewer/sdl2-viewer/build" -j

echo "[build] viewer/raylib-viewer"
cmake -S "$ROOT_DIR/viewer/raylib-viewer" -B "$ROOT_DIR/viewer/raylib-viewer/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/viewer/raylib-viewer/build" -j

echo "[build] viewer/tkinter-viewer"
(cd "$ROOT_DIR/viewer/tkinter-viewer" && uv sync)

echo "[run] renderer"
"$ROOT_DIR/renderer/build/timestamp_writer" "$SHM_NAME" &
renderer_pid=$!

echo "[run] viewer/sdl2-viewer"
"$ROOT_DIR/viewer/sdl2-viewer/build/sdl2_viewer" "$SHM_NAME" &
sdl2_pid=$!

echo "[run] viewer/raylib-viewer"
"$ROOT_DIR/viewer/raylib-viewer/build/raylib_viewer" "$SHM_NAME" &
raylib_pid=$!

echo "[run] viewer/tkinter-viewer"
(cd "$ROOT_DIR/viewer/tkinter-viewer" && uv run main.py "$SHM_NAME") &
tkinter_pid=$!

wait -n "$renderer_pid" "$sdl2_pid" "$raylib_pid" "$tkinter_pid"

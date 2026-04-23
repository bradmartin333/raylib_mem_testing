#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHM_NAME="${1:-/raylib_fb_rgb565}"

cleanup() {
	local pids=("${renderer_pid:-}" "${cpp_pid:-}" "${python_pid:-}")
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

echo "[build] viewer/cpp"
cmake -S "$ROOT_DIR/viewer/cpp" -B "$ROOT_DIR/viewer/cpp/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/viewer/cpp/build" -j

echo "[build] viewer/python"
(cd "$ROOT_DIR/viewer/python" && uv sync)

echo "[run] renderer"
"$ROOT_DIR/renderer/build/timestamp_writer" "$SHM_NAME" &
renderer_pid=$!

echo "[run] viewer/cpp"
"$ROOT_DIR/viewer/cpp/build/shm_viewer_cpp" "$SHM_NAME" &
cpp_pid=$!

echo "[run] viewer/python"
(cd "$ROOT_DIR/viewer/python" && uv run main.py "$SHM_NAME") &
python_pid=$!

wait -n "$renderer_pid" "$cpp_pid" "$python_pid"

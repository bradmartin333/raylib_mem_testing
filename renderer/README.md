# raylib_mem_testing

Two-process demo using shared memory:

- `timestamp_writer` (C): software-renders a high-resolution timestamp using raylib image APIs and publishes RGB565 frames to POSIX shared memory.
- `framebuffer_viewer` (C++): reads the shared memory framebuffer and displays it in a raylib window with basic diagnostics.

## Current defaults

- Shared memory name: `/raylib_fb_rgb565`
- Resolution: `320x240`
- Pixel format: `RGB565`
- Sync model: lock-free latest-frame publishing (writer increments frame counter each published frame)

## Prerequisites (Linux)

- CMake >= 3.25
- C compiler and C++ compiler (for example `gcc` and `g++`)
- X11/OpenGL runtime dependencies required by raylib

Example Ubuntu install:

```bash
sudo apt update
sudo apt install -y build-essential cmake libx11-dev libxcursor-dev libxrandr-dev libxi-dev libxinerama-dev libgl1-mesa-dev libasound2-dev libwayland-dev libxkbcommon-dev
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

Terminal 1:

```bash
./build/timestamp_writer /raylib_fb_rgb565
```

Terminal 2:

```bash
./build/framebuffer_viewer /raylib_fb_rgb565
```

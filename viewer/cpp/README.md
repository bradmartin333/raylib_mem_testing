# viewer/cpp

SDL2 native window viewer for the shared-memory RGB565 framebuffer.

## Prerequisites

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev pkg-config
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/shm_viewer_cpp /raylib_fb_rgb565
```
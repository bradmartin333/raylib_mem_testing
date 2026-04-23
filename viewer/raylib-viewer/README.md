# viewer/raylib-viewer

raylib native window viewer for the shared-memory RGB565 framebuffer.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/raylib_viewer /raylib_fb_rgb565
```

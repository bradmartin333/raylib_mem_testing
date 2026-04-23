# viewer

Native shared-memory viewers for the RGB565 framebuffer written by `/renderer/build/timestamp_writer`.

- `/viewer/sdl2-viewer`: SDL2-based C++ viewer
- `/viewer/raylib-viewer`: raylib-based C++ viewer
- `/viewer/tkinter-viewer`: Tkinter + Pillow viewer managed with `uv`

All viewers expect the shared-memory contract defined in `/shared/shm_layout.h` and default to `/raylib_fb_rgb565`.
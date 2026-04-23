# viewer

Native shared-memory viewers for the RGB565 framebuffer written by `/renderer/build/timestamp_writer`.

- `/viewer/cpp`: SDL2-based C++ viewer
- `/viewer/python`: Tkinter + Pillow viewer managed with `uv`

Both viewers expect the shared-memory contract defined in `/shared/shm_layout.h` and default to `/raylib_fb_rgb565`.
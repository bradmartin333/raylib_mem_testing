# viewer/python

Tkinter + Pillow native window viewer for the shared-memory RGB565 framebuffer.

## Prerequisites

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y python3-tk
```

## Install and Run With uv

```bash
uv sync
uv run python -m shm_viewer /raylib_fb_rgb565
```
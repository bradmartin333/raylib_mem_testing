# viewer/tkinter-viewer

Tkinter + Pillow native window viewer for the shared-memory RGB565 framebuffer.

## Prerequisites

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y python3-tk
```

## Install and Run With uv

Install `uv` if needed:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Then run the viewer:

```bash
uv sync
uv run main.py /raylib_fb_rgb565
```
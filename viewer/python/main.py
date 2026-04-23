from __future__ import annotations

import argparse
import mmap
import struct
import sys
import time
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageTk

SHM_DEFAULT_NAME = "/raylib_fb_rgb565"
SHM_MAGIC = 0x52464231
SHM_VERSION = 1
SHM_PIXEL_RGB565 = 1
SHM_TEXT_CAPACITY = 96
OVERLAY_HEIGHT = 78
HEADER_STRUCT = struct.Struct("<IIIIIIQQ96s")


@dataclass(slots=True)
class Header:
    magic: int
    version: int
    width: int
    height: int
    pixel_format: int
    stride_bytes: int
    frame_counter: int
    timestamp_ns: int
    timestamp_text: str


class SharedMemoryFramebuffer:
    def __init__(self, shm_name: str) -> None:
        self.shm_name = shm_name
        self.path = Path("/dev/shm") / shm_name.lstrip("/")
        self.file = self.path.open("rb")
        self.mapping = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)

    def close(self) -> None:
        self.mapping.close()
        self.file.close()

    def read_header(self) -> Header:
        fields = HEADER_STRUCT.unpack_from(self.mapping, 0)
        raw_text = fields[8].split(b"\0", 1)[0]
        return Header(
            magic=fields[0],
            version=fields[1],
            width=fields[2],
            height=fields[3],
            pixel_format=fields[4],
            stride_bytes=fields[5],
            frame_counter=fields[6],
            timestamp_ns=fields[7],
            timestamp_text=raw_text.decode("utf-8", errors="replace"),
        )

    def read_frame(self) -> tuple[Header, bytes]:
        header = self.read_header()
        expected_size = HEADER_STRUCT.size + header.height * header.stride_bytes
        if expected_size > len(self.mapping):
            raise RuntimeError("shared memory payload is truncated")
        payload = self.mapping[HEADER_STRUCT.size : expected_size]
        return header, payload


def rgb565_to_rgb888(
    payload: bytes, width: int, height: int, stride_bytes: int
) -> bytes:
    rgb = bytearray(width * height * 3)
    out_index = 0
    for y in range(height):
        row_offset = y * stride_bytes
        for x in range(width):
            pixel_offset = row_offset + x * 2
            pixel = payload[pixel_offset] | (payload[pixel_offset + 1] << 8)
            rgb[out_index] = ((pixel >> 11) & 0x1F) * 255 // 31
            rgb[out_index + 1] = ((pixel >> 5) & 0x3F) * 255 // 63
            rgb[out_index + 2] = (pixel & 0x1F) * 255 // 31
            out_index += 3
    return bytes(rgb)


class ViewerApp:
    def __init__(self, shm: SharedMemoryFramebuffer) -> None:
        self.shm = shm
        self.root = tk.Tk()
        self.root.title("Shared Memory Viewer (Tkinter)")
        self.root.geometry("640x558")
        self.root.minsize(320, 240 + OVERLAY_HEIGHT)

        self.canvas = tk.Canvas(self.root, bg="#101218", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.photo: ImageTk.PhotoImage | None = None
        self.last_frame = -1
        self.last_new_frame_time = time.monotonic()
        self.cached_image: Image.Image | None = None
        self.after_id: str | None = None

        self.root.bind("<Configure>", self._on_resize)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _on_resize(self, _event: tk.Event[tk.Misc]) -> None:
        if self.cached_image is not None:
            self.render(force=False, schedule_next=False)

    def _on_close(self) -> None:
        if self.after_id is not None:
            self.root.after_cancel(self.after_id)
            self.after_id = None
        self.shm.close()
        self.root.destroy()

    def render(self, force: bool = False, schedule_next: bool = True) -> None:
        header, payload = self.shm.read_frame()

        if header.magic != SHM_MAGIC or header.version != SHM_VERSION:
            raise RuntimeError("shared memory format mismatch")
        if header.pixel_format != SHM_PIXEL_RGB565:
            raise RuntimeError(f"unsupported pixel format: {header.pixel_format}")
        if header.stride_bytes < header.width * 2:
            raise RuntimeError("stride is too small for RGB565 data")

        if (
            force
            or header.frame_counter != self.last_frame
            or self.cached_image is None
        ):
            rgb_bytes = rgb565_to_rgb888(
                payload, header.width, header.height, header.stride_bytes
            )
            self.cached_image = Image.frombytes(
                "RGB", (header.width, header.height), rgb_bytes
            )
            self.last_frame = header.frame_counter
            self.last_new_frame_time = time.monotonic()

        canvas_width = max(self.canvas.winfo_width(), 1)
        canvas_height = max(self.canvas.winfo_height(), 1)
        frame_area_height = max(canvas_height - OVERLAY_HEIGHT, 1)

        scale = min(canvas_width / header.width, frame_area_height / header.height)
        render_width = max(int(header.width * scale), 1)
        render_height = max(int(header.height * scale), 1)
        offset_x = (canvas_width - render_width) // 2
        offset_y = (frame_area_height - render_height) // 2

        scaled = self.cached_image.resize(
            (render_width, render_height), Image.Resampling.NEAREST
        )
        self.photo = ImageTk.PhotoImage(scaled)

        self.canvas.delete("all")
        self.canvas.create_image(offset_x, offset_y, anchor=tk.NW, image=self.photo)
        self.canvas.create_rectangle(
            0,
            frame_area_height,
            canvas_width,
            canvas_height,
            fill="#000000",
            outline="",
        )

        stale_seconds = time.monotonic() - self.last_new_frame_time
        stale_color = "#ffb040" if stale_seconds > 0.5 else "#8cffaa"

        self.canvas.create_text(
            12,
            frame_area_height + 16,
            anchor=tk.NW,
            fill="#f5f5f5",
            font=("TkFixedFont", 12, "bold"),
            text=f"Frame: {header.frame_counter}",
        )
        self.canvas.create_text(
            12,
            frame_area_height + 36,
            anchor=tk.NW,
            fill="#aae6ff",
            font=("TkFixedFont", 11),
            text=f"Timestamp: {header.timestamp_text}",
        )
        self.canvas.create_text(
            12,
            frame_area_height + 58,
            anchor=tk.NW,
            fill=stale_color,
            font=("TkFixedFont", 11),
            text=f"Stale: {stale_seconds:.3f} s",
        )

        if schedule_next:
            self.after_id = self.root.after(16, self.render)

    def run(self) -> None:
        self.render(force=True, schedule_next=True)
        self.root.mainloop()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Tkinter shared-memory framebuffer viewer"
    )
    parser.add_argument("shm_name", nargs="?", default=SHM_DEFAULT_NAME)
    args = parser.parse_args(argv)

    try:
        shm = SharedMemoryFramebuffer(args.shm_name)
    except FileNotFoundError:
        print(
            f"shared memory object not found: /dev/shm/{args.shm_name.lstrip('/')}",
            file=sys.stderr,
        )
        return 1

    try:
        app = ViewerApp(shm)
        app.run()
    except Exception as exc:
        shm.close()
        print(f"viewer failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

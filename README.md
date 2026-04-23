# raylib_mem_testing

Shared-memory framebuffer demo with one renderer and multiple viewers.

https://github.com/user-attachments/assets/c502a655-8ff2-44b4-b230-02b71ed7e1be

## TODO

- [ ] Add synchronization primitives to the shared-memory contract and implement a lock-based viewer that waits for new frames instead of polling.
- [ ] Check for locally installed raylib and SDL2 versions and use those if available instead of vendored copies.

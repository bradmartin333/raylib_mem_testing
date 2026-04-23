#include "shm_layout.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "raylib.h"

struct ViewerState {
    int fd = -1;
    void *mapped = nullptr;
    size_t map_size = 0;
    const ShmFramebufferHeader *header = nullptr;
    const uint16_t *pixels = nullptr;
};

static bool map_shared_buffer(const char *shm_name, ViewerState *state)
{
    state->fd = shm_open(shm_name, O_RDONLY, 0);
    if (state->fd < 0) {
        std::fprintf(stderr, "shm_open failed for %s: %s\n", shm_name, std::strerror(errno));
        return false;
    }

    struct stat st;
    if (fstat(state->fd, &st) != 0) {
        std::fprintf(stderr, "fstat failed: %s\n", std::strerror(errno));
        close(state->fd);
        state->fd = -1;
        return false;
    }

    state->map_size = (size_t)st.st_size;
    state->mapped = mmap(nullptr, state->map_size, PROT_READ, MAP_SHARED, state->fd, 0);
    if (state->mapped == MAP_FAILED) {
        std::fprintf(stderr, "mmap failed: %s\n", std::strerror(errno));
        close(state->fd);
        state->fd = -1;
        state->mapped = nullptr;
        return false;
    }

    state->header = (const ShmFramebufferHeader *)state->mapped;
    state->pixels = (const uint16_t *)shm_pixels(state->mapped);
    return true;
}

static void unmap_shared_buffer(ViewerState *state)
{
    if (state->mapped && state->mapped != MAP_FAILED) {
        munmap(state->mapped, state->map_size);
    }
    if (state->fd >= 0) {
        close(state->fd);
    }
    *state = ViewerState{};
}

static Color rgb565_to_rgba(uint16_t p)
{
    Color c;
    c.r = (unsigned char)(((p >> 11) & 0x1F) * 255 / 31);
    c.g = (unsigned char)(((p >> 5) & 0x3F) * 255 / 63);
    c.b = (unsigned char)((p & 0x1F) * 255 / 31);
    c.a = 255;
    return c;
}

int main(int argc, char **argv)
{
    const char *shm_name = (argc > 1) ? argv[1] : SHM_DEFAULT_NAME;

    ViewerState state;
    if (!map_shared_buffer(shm_name, &state)) {
        return 1;
    }

    if (state.header->magic != SHM_MAGIC || state.header->version != SHM_VERSION) {
        std::fprintf(stderr, "shared memory format mismatch\n");
        unmap_shared_buffer(&state);
        return 1;
    }

    if (state.header->pixel_format != SHM_PIXEL_RGB565) {
        std::fprintf(stderr, "unsupported pixel format: %u\n", state.header->pixel_format);
        unmap_shared_buffer(&state);
        return 1;
    }

    const int width = (int)state.header->width;
    const int height = (int)state.header->height;
    const int win_w = width * 2;
    const int win_h = height * 2;

    InitWindow(win_w, win_h, "Shared Memory Viewer");
    SetTargetFPS(120);

    Image image = GenImageColor(width, height, BLACK);
    Texture2D texture = LoadTextureFromImage(image);
    Color *rgba_pixels = (Color *)image.data;

    unsigned long long last_frame = 0;
    double last_new_frame_time = GetTime();

    while (!WindowShouldClose()) {
        unsigned long long frame = __atomic_load_n(&state.header->frame_counter, __ATOMIC_ACQUIRE);
        if (frame != last_frame) {
            size_t count = (size_t)width * (size_t)height;
            for (size_t i = 0; i < count; ++i) {
                rgba_pixels[i] = rgb565_to_rgba(state.pixels[i]);
            }
            UpdateTexture(texture, image.data);
            last_frame = frame;
            last_new_frame_time = GetTime();
        }

        double stale_s = GetTime() - last_new_frame_time;

        BeginDrawing();
        ClearBackground((Color){16, 18, 24, 255});

        Rectangle src = {0.0f, 0.0f, (float)width, (float)height};
        Rectangle dst = {0.0f, 0.0f, (float)win_w, (float)win_h};
        DrawTexturePro(texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);

        DrawRectangle(0, win_h - 80, win_w, 80, (Color){0, 0, 0, 140});
        DrawText(TextFormat("Frame: %llu", frame), 10, win_h - 72, 20, RAYWHITE);
        DrawText(TextFormat("Timestamp: %s", state.header->timestamp_text), 10, win_h - 46, 18, (Color){170, 230, 255, 255});
        DrawText(TextFormat("Stale: %.3f s", stale_s), 10, win_h - 24, 18, stale_s > 0.5 ? ORANGE : (Color){140, 255, 170, 255});

        EndDrawing();
    }

    UnloadTexture(texture);
    UnloadImage(image);
    CloseWindow();

    unmap_shared_buffer(&state);
    return 0;
}

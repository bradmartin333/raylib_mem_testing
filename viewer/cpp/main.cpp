#include "shm_layout.h"

#include <SDL.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct ViewerState {
    int fd = -1;
    void *mapped = nullptr;
    size_t map_size = 0;
    const ShmFramebufferHeader *header = nullptr;
    const uint8_t *pixels = nullptr;
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

    if (st.st_size < (off_t)sizeof(ShmFramebufferHeader)) {
        std::fprintf(stderr, "shared memory region is too small\n");
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

    state->header = static_cast<const ShmFramebufferHeader *>(state->mapped);
    state->pixels = shm_pixels(state->mapped);
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

static bool validate_layout(const ViewerState &state)
{
    if (state.header->magic != SHM_MAGIC || state.header->version != SHM_VERSION) {
        std::fprintf(stderr, "shared memory format mismatch\n");
        return false;
    }

    if (state.header->pixel_format != SHM_PIXEL_RGB565) {
        std::fprintf(stderr, "unsupported pixel format: %u\n", state.header->pixel_format);
        return false;
    }

    const size_t expected_size = shm_total_size(state.header->width, state.header->height, state.header->stride_bytes);
    if (expected_size > state.map_size) {
        std::fprintf(stderr, "shared memory payload is truncated\n");
        return false;
    }

    if (state.header->stride_bytes < state.header->width * sizeof(uint16_t)) {
        std::fprintf(stderr, "stride is too small for RGB565 data\n");
        return false;
    }

    return true;
}

static uint32_t rgb565_to_argb8888(uint16_t pixel)
{
    const uint32_t red = ((pixel >> 11) & 0x1F) * 255u / 31u;
    const uint32_t green = ((pixel >> 5) & 0x3F) * 255u / 63u;
    const uint32_t blue = (pixel & 0x1F) * 255u / 31u;
    return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

static std::array<uint8_t, 7> glyph_rows(char c)
{
    switch (c) {
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, SDL_Color color, const std::string &text)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int cursor_x = x;
    for (char c : text) {
        const auto rows = glyph_rows(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (1u << (4 - col))) == 0) {
                    continue;
                }

                SDL_Rect pixel = {
                    cursor_x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                };
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
        cursor_x += 6 * scale;
    }
}

static SDL_Rect fit_rect(int frame_width, int frame_height, int window_width, int window_height)
{
    const float scale_x = (float)window_width / (float)frame_width;
    const float scale_y = (float)window_height / (float)frame_height;
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const int dest_width = (int)((float)frame_width * scale);
    const int dest_height = (int)((float)frame_height * scale);
    return SDL_Rect{
        (window_width - dest_width) / 2,
        (window_height - dest_height) / 2,
        dest_width,
        dest_height,
    };
}

int main(int argc, char **argv)
{
    const char *shm_name = (argc > 1) ? argv[1] : SHM_DEFAULT_NAME;

    ViewerState state;
    if (!map_shared_buffer(shm_name, &state)) {
        return 1;
    }

    if (!validate_layout(state)) {
        unmap_shared_buffer(&state);
        return 1;
    }

    const int width = (int)state.header->width;
    const int height = (int)state.header->height;
    const int overlay_height = 84;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        unmap_shared_buffer(&state);
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Window *window = SDL_CreateWindow(
        "Shared Memory Viewer (SDL2)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width * 2,
        height * 2 + overlay_height,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        unmap_shared_buffer(&state);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        unmap_shared_buffer(&state);
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        unmap_shared_buffer(&state);
        return 1;
    }

    std::vector<uint32_t> rgba_pixels((size_t)width * (size_t)height, 0xFF000000u);
    uint64_t last_frame = 0;
    auto last_new_frame_time = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        const uint64_t frame = __atomic_load_n(&state.header->frame_counter, __ATOMIC_ACQUIRE);
        if (frame != last_frame) {
            const uint32_t stride_bytes = state.header->stride_bytes;
            for (int y = 0; y < height; ++y) {
                const auto *src_row = reinterpret_cast<const uint16_t *>(state.pixels + (size_t)y * stride_bytes);
                auto *dst_row = rgba_pixels.data() + (size_t)y * (size_t)width;
                for (int x = 0; x < width; ++x) {
                    dst_row[x] = rgb565_to_argb8888(src_row[x]);
                }
            }

            SDL_UpdateTexture(texture, nullptr, rgba_pixels.data(), width * (int)sizeof(uint32_t));
            last_frame = frame;
            last_new_frame_time = std::chrono::steady_clock::now();
        }

        const auto now = std::chrono::steady_clock::now();
        const double stale_seconds = std::chrono::duration<double>(now - last_new_frame_time).count();

        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);

        const int frame_area_height = (window_height > overlay_height) ? (window_height - overlay_height) : window_height;
        const SDL_Rect dest = fit_rect(width, height, window_width, frame_area_height);

        SDL_SetRenderDrawColor(renderer, 16, 18, 24, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &dest);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect overlay = {0, window_height - overlay_height, window_width, overlay_height};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &overlay);

        char frame_line[64];
        char stamp_line[160];
        char stale_line[64];
        std::snprintf(frame_line, sizeof(frame_line), "FRAME %llu", (unsigned long long)frame);
        std::snprintf(stamp_line, sizeof(stamp_line), "STAMP %s", state.header->timestamp_text);
        std::snprintf(stale_line, sizeof(stale_line), "STALE %.3fS", stale_seconds);

        draw_text(renderer, 12, window_height - overlay_height + 10, 3, SDL_Color{245, 245, 245, 255}, frame_line);
        draw_text(renderer, 12, window_height - overlay_height + 34, 2, SDL_Color{170, 230, 255, 255}, stamp_line);
        draw_text(renderer, 12, window_height - overlay_height + 56, 2, stale_seconds > 0.5 ? SDL_Color{255, 176, 64, 255} : SDL_Color{140, 255, 170, 255}, stale_line);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    unmap_shared_buffer(&state);
    return 0;
}
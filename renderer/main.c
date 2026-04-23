#include "shm_layout.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "raylib.h"

static volatile sig_atomic_t g_keep_running = 1;

typedef struct Ball {
    float x;
    float y;
    float vx;
    float vy;
    int radius;
    Color color;
} Ball;

#define BALL_RADIUS 12

static void handle_signal(int sig)
{
    (void)sig;
    g_keep_running = 0;
}

static uint64_t realtime_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void format_timestamp(char *buffer, size_t cap, uint64_t now_ns)
{
    time_t sec = (time_t)(now_ns / 1000000000ull);
    long ns = (long)(now_ns % 1000000000ull);

    struct tm local_tm;
    localtime_r(&sec, &local_tm);

    char base[64];
    strftime(base, sizeof(base), "%Y-%m-%d %H:%M:%S", &local_tm);
    snprintf(buffer, cap, "%s.%09ld", base, ns);
}

static uint16_t rgba_to_rgb565(Color c)
{
    uint16_t r = (uint16_t)(c.r >> 3);
    uint16_t g = (uint16_t)(c.g >> 2);
    uint16_t b = (uint16_t)(c.b >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void convert_rgba_to_rgb565(const Color *src, uint16_t *dst, uint32_t width, uint32_t height)
{
    size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; ++i) {
        dst[i] = rgba_to_rgb565(src[i]);
    }
}

static void update_balls(Ball *balls, size_t count, float dt_s, int width, int height)
{
    for (size_t i = 0; i < count; ++i) {
        Ball *ball = &balls[i];
        ball->x += ball->vx * dt_s;
        ball->y += ball->vy * dt_s;

        if (ball->x - (float)ball->radius < 0.0f) {
            ball->x = (float)ball->radius;
            ball->vx = -ball->vx;
        } else if (ball->x + (float)ball->radius > (float)width) {
            ball->x = (float)width - (float)ball->radius;
            ball->vx = -ball->vx;
        }

        if (ball->y - (float)ball->radius < 0.0f) {
            ball->y = (float)ball->radius;
            ball->vy = -ball->vy;
        } else if (ball->y + (float)ball->radius > (float)height) {
            ball->y = (float)height - (float)ball->radius;
            ball->vy = -ball->vy;
        }
    }
}

static void draw_balls(Image *canvas, const Ball *balls, size_t count)
{
    // Rasterize each ball as horizontal spans of a circle equation.
    for (size_t i = 0; i < count; ++i) {
        int cx = (int)balls[i].x;
        int cy = (int)balls[i].y;
        int radius = balls[i].radius;
        int r2 = radius * radius;

        for (int y = -radius; y <= radius; ++y) {
            int inside = r2 - (y * y);
            int x = (int)sqrtf((float)inside);
            ImageDrawLine(canvas, cx - x, cy + y, cx + x, cy + y, balls[i].color);
        }
    }
}

int main(int argc, char **argv)
{
    const char *shm_name = (argc > 1) ? argv[1] : SHM_DEFAULT_NAME;
    const uint32_t width = SHM_DEFAULT_WIDTH;
    const uint32_t height = SHM_DEFAULT_HEIGHT;
    const uint32_t stride_bytes = width * sizeof(uint16_t);
    const size_t map_size = shm_total_size(width, height, stride_bytes);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        fprintf(stderr, "shm_open failed for %s: %s\n", shm_name, strerror(errno));
        return 1;
    }

    if (ftruncate(fd, (off_t)map_size) != 0) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    void *mapped = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    ShmFramebufferHeader *header = (ShmFramebufferHeader *)mapped;
    uint16_t *pixel_dst = (uint16_t *)shm_pixels_mut(mapped);

    memset(mapped, 0, map_size);
    header->magic = SHM_MAGIC;
    header->version = SHM_VERSION;
    header->width = width;
    header->height = height;
    header->pixel_format = SHM_PIXEL_RGB565;
    header->stride_bytes = stride_bytes;

    Image canvas = GenImageColor((int)width, (int)height, (Color){20, 22, 30, 255});
    Ball balls[] = {
        {52.0f, 60.0f, 62.0f, 44.0f, BALL_RADIUS, (Color){255, 96, 96, 255}},
        {128.0f, 84.0f, -56.0f, 52.0f, BALL_RADIUS, (Color){96, 196, 255, 255}},
        {205.0f, 136.0f, 70.0f, -48.0f, BALL_RADIUS, (Color){132, 255, 172, 255}},
        {270.0f, 180.0f, -74.0f, -62.0f, BALL_RADIUS, (Color){255, 222, 92, 255}},
        {158.0f, 200.0f, 48.0f, -70.0f, BALL_RADIUS, (Color){220, 152, 255, 255}},
    };
    const size_t ball_count = sizeof(balls) / sizeof(balls[0]);
    uint64_t prev_tick_ns = realtime_ns();

    printf("timestamp_writer publishing to %s (%ux%u, RGB565)\n", shm_name, width, height);

    while (g_keep_running) {
        uint64_t now_ns = realtime_ns();
        float dt_s = (float)((double)(now_ns - prev_tick_ns) / 1000000000.0);
        if (dt_s < 0.0f) {
            dt_s = 0.0f;
        }
        if (dt_s > 0.05f) {
            dt_s = 0.05f;
        }
        prev_tick_ns = now_ns;

        char text[SHM_TEXT_CAPACITY];
        format_timestamp(text, sizeof(text), now_ns);

        update_balls(balls, ball_count, dt_s, (int)width, (int)height);

        ImageClearBackground(&canvas, (Color){20, 22, 30, 255});
        draw_balls(&canvas, balls, ball_count);
        ImageDrawText(&canvas, text, 10, (int)(height / 2) - 10, 20, (Color){120, 255, 160, 255});

        convert_rgba_to_rgb565((const Color *)canvas.data, pixel_dst, width, height);

        __atomic_store_n(&header->timestamp_ns, now_ns, __ATOMIC_RELEASE);
        strncpy(header->timestamp_text, text, SHM_TEXT_CAPACITY - 1);
        header->timestamp_text[SHM_TEXT_CAPACITY - 1] = '\0';
        __atomic_add_fetch(&header->frame_counter, 1ull, __ATOMIC_RELEASE);

        struct timespec nap = {.tv_sec = 0, .tv_nsec = 1000000L};
        nanosleep(&nap, NULL);
    }

    UnloadImage(canvas);
    munmap(mapped, map_size);
    close(fd);
    return 0;
}

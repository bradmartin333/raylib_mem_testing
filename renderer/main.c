#include "shm_layout.h"

#include <errno.h>
#include <fcntl.h>
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

    printf("timestamp_writer publishing to %s (%ux%u, RGB565)\n", shm_name, width, height);

    while (g_keep_running) {
        uint64_t now_ns = realtime_ns();
        char text[SHM_TEXT_CAPACITY];
        format_timestamp(text, sizeof(text), now_ns);

        ImageClearBackground(&canvas, (Color){20, 22, 30, 255});
        ImageDrawRectangle(&canvas, 0, 0, (int)width, 28, (Color){34, 42, 64, 255});
        ImageDrawText(&canvas, "raylib software render -> shared memory", 8, 8, 12, (Color){183, 214, 255, 255});
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

#ifndef SHM_LAYOUT_H
#define SHM_LAYOUT_H

#include <stdint.h>
#include <stddef.h>

#define SHM_DEFAULT_NAME "/raylib_fb_rgb565"
#define SHM_MAGIC 0x52464231u
#define SHM_VERSION 1u
#define SHM_TEXT_CAPACITY 96u
#define SHM_DEFAULT_WIDTH 320u
#define SHM_DEFAULT_HEIGHT 240u

enum {
    SHM_PIXEL_RGB565 = 1
};

typedef struct ShmFramebufferHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t stride_bytes;
    uint64_t frame_counter;
    uint64_t timestamp_ns;
    char timestamp_text[SHM_TEXT_CAPACITY];
} ShmFramebufferHeader;

static inline size_t shm_payload_size(uint32_t width, uint32_t height, uint32_t stride_bytes)
{
    return (size_t)height * (size_t)stride_bytes;
}

static inline size_t shm_total_size(uint32_t width, uint32_t height, uint32_t stride_bytes)
{
    return sizeof(ShmFramebufferHeader) + shm_payload_size(width, height, stride_bytes);
}

static inline uint8_t *shm_pixels_mut(void *base)
{
    return (uint8_t *)base + sizeof(ShmFramebufferHeader);
}

static inline const uint8_t *shm_pixels(const void *base)
{
    return (const uint8_t *)base + sizeof(ShmFramebufferHeader);
}

#endif

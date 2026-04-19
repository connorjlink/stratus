#ifndef STRATUS_VIRTIO_GPU_H
#define STRATUS_VIRTIO_GPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
    uint32_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
} FramebufferInfo;

bool virtio_gpu_init(FramebufferInfo* out_fb);
bool virtio_gpu_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#endif

#ifndef STRATUS_VIRTIO_MMIO_H
#define STRATUS_VIRTIO_MMIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
    uintptr_t base;
    uint32_t version;
} ViMMIODevice;

bool virtio_mmio_find_device(uint32_t device_id, ViMMIODevice* out_dev);
bool virtio_mmio_init(ViMMIODevice* device);
uint32_t virtio_mmio_read_device_features(ViMMIODevice* device, uint32_t sel);
void virtio_mmio_write_driver_features(ViMMIODevice* device, uint32_t sel, uint32_t value);
bool virtio_mmio_negotiate(ViMMIODevice* device, uint64_t wanted_features, uint64_t* out_accepted);

#define VIRTQ_DESC_F_NEXT  1u
#define VIRTQ_DESC_F_WRITE 2u

typedef struct
{
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VqDescriptor;

typedef struct
{
    uint16_t flags;
    uint16_t index;
    uint16_t ring[];
} VqAvailable;

typedef struct
{
    uint32_t id;
    uint32_t length;
} VqConsumedElement;

typedef struct
{
    uint16_t flags;
    uint16_t index;
    VqConsumedElement ring[];
} VqConsumed;

typedef struct
{
    ViMMIODevice* device;
    uint16_t queue_size;

    VqDescriptor* descriptor;
    VqAvailable* available;
    VqConsumed* used;

    uint16_t free_head;
    uint16_t number_free;

    uint16_t last_used_index;

    uint16_t* free_next;
} ViQueue;

bool virtq_init(ViMMIODevice* device, uint32_t queue_index, uint16_t queue_size, ViQueue* out_q);
int virtq_alloc_chain(ViQueue* q, uint16_t count);
void virtq_free_chain(ViQueue* q, uint16_t head);
void virtq_submit(ViQueue* q, uint16_t head);
bool virtq_poll_used(ViQueue* q, uint16_t* out_id);
void virtio_mmio_notify_queue(ViMMIODevice* device, uint32_t queue_index);

#endif

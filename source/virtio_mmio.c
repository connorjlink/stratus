#include "virtio_mmio.h"
#include "memory.h"
#include "utility.h"

// virtIO-MMIO register offsets
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070

// modern (version 2) queue addresses
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW  0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH 0x0a4

#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc

// legacy (version 1)
#define VIRTIO_MMIO_GUEST_PAGE_SIZE  0x028
#define VIRTIO_MMIO_QUEUE_ALIGN      0x03c
#define VIRTIO_MMIO_QUEUE_PFN        0x040

// status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER      2u
#define VIRTIO_STATUS_DRIVER_OK   4u
#define VIRTIO_STATUS_FEATURES_OK 8u
#define VIRTIO_STATUS_FAILED      128u

// feature bits
#define VIRTIO_F_VERSION_1 (1ull << 32)

static inline void fence_iorw(void)
{
#if defined(__riscv)
    __asm__ volatile ("fence iorw, iorw" : : : "memory");
#else
    (void)0;
#endif
}

static inline uint32_t mmio_read32(uintptr_t base, uint32_t offset)
{
    return *(volatile uint32_t*)(base + offset);
}

static inline void mmio_write32(uintptr_t base, uint32_t offset, uint32_t v)
{
    *(volatile uint32_t*)(base + offset) = v;
}

static inline void mmio_write64_split(uintptr_t base, uint32_t offset_low, uint64_t v)
{
    mmio_write32(base, offset_low, (uint32_t)(v & 0xffffffffu));
    mmio_write32(base, offset_low + 4u, (uint32_t)(v >> 32));
}

static inline size_t align_up_size(size_t v, size_t align)
{
    if (align == 0) 
    {
        return v;
    }

    return (v + (align - 1)) & ~(align - 1);
}

bool virtio_mmio_find_device(uint32_t device_id, ViMMIODevice* output_device)
{
    if (!output_device) 
    {
        return false;
    }

    const uintptr_t start = 0x10001000u;
    const uintptr_t stride = 0x1000u;
    const unsigned max_slots = 32;

    for (unsigned i = 0; i < max_slots; i++)
    {
        uintptr_t base = start + (uintptr_t)i * stride;
        uint32_t magic = mmio_read32(base, VIRTIO_MMIO_MAGIC_VALUE);
        // virt device QEMU
        if (magic != 0x74726976u)
        {
            continue;
        }

        uint32_t mmio_device_id = mmio_read32(base, VIRTIO_MMIO_DEVICE_ID);
        if (mmio_device_id != device_id)
        {
            continue;
        }

        output_device->base = base;
        output_device->version = mmio_read32(base, VIRTIO_MMIO_VERSION);
        return true;
    }

    return false;
}

bool virtio_mmio_init(ViMMIODevice* device)
{
    if (!device) 
    {
        return false;
    }

    if (mmio_read32(device->base, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976u)
    {
        return false;
    }

    if (mmio_read32(device->base, VIRTIO_MMIO_DEVICE_ID) == 0)
    {
        return false;
    }

    device->version = mmio_read32(device->base, VIRTIO_MMIO_VERSION);

    mmio_write32(device->base, VIRTIO_MMIO_STATUS, 0);
    fence_iorw();

    mmio_write32(device->base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    mmio_write32(device->base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    return true;
}

uint32_t virtio_mmio_read_device_features(ViMMIODevice* device, uint32_t selector)
{
    mmio_write32(device->base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, selector);
    fence_iorw();
    return mmio_read32(device->base, VIRTIO_MMIO_DEVICE_FEATURES);
}

void virtio_mmio_write_driver_features(ViMMIODevice* device, uint32_t selector, uint32_t value)
{
    mmio_write32(device->base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, selector);
    fence_iorw();
    mmio_write32(device->base, VIRTIO_MMIO_DRIVER_FEATURES, value);
    fence_iorw();
}

bool virtio_mmio_negotiate(ViMMIODevice* device, uint64_t wanted_features, uint64_t* output_accepted)
{
    uint64_t host = 0;
    host |= (uint64_t)virtio_mmio_read_device_features(device, 0);
    host |= (uint64_t)virtio_mmio_read_device_features(device, 1) << 32;

    uint64_t accepted = host & wanted_features;

    virtio_mmio_write_driver_features(device, 0, (uint32_t)(accepted & 0xffffffffu));
    virtio_mmio_write_driver_features(device, 1, (uint32_t)(accepted >> 32));

    uint32_t status = mmio_read32(device->base, VIRTIO_MMIO_STATUS);
    mmio_write32(device->base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FEATURES_OK);
    fence_iorw();

    status = mmio_read32(device->base, VIRTIO_MMIO_STATUS);
    if ((status & VIRTIO_STATUS_FEATURES_OK) == 0)
    {
        mmio_write32(device->base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return false;
    }

    if (output_accepted) 
    {
        *output_accepted = accepted;
    }
    return true;
}

static void virtq_init_free_list(ViQueue* queue)
{
    queue->free_head = 0;
    queue->number_free = queue->queue_size;

    for (uint16_t i = 0; i < queue->queue_size; i++)
    {
        queue->free_next[i] = (uint16_t)(i + 1);
    }

    queue->free_next[queue->queue_size - 1] = 0xffffu;
}

bool virtq_init(ViMMIODevice* device, uint32_t queue_index, uint16_t queue_size, ViQueue* output_queue)
{
    if (!device || !output_queue || queue_size == 0 || device->base == 0)
    {
        return false;
    }

    printf("virtq_init: device@0x%x base=0x%x ver=%u queue=%u\n",
           (unsigned)(uintptr_t)device,
           (unsigned)device->base,
           (unsigned)device->version,
           (unsigned)queue_index);

    mmio_write32(device->base, VIRTIO_MMIO_QUEUE_SEL, queue_index);
    fence_iorw();

    printf("virtq_init: queue_sel ok\n");

    uint32_t maximum = mmio_read32(device->base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (maximum == 0) 
    {
        return false;
    }

    printf("virtq_init: qmax=%u\n", (unsigned)maximum);

    if (queue_size > maximum) 
    {
        queue_size = (uint16_t)maximum;
    }

    if (queue_size > 64) 
    {
        queue_size = 64;
    }

    size_t descriptor_bytes = sizeof(VqDescriptor) * queue_size;
    size_t available_bytes = sizeof(VqAvailable) + sizeof(uint16_t) * (queue_size + 1);
    size_t used_bytes = sizeof(VqConsumed) + sizeof(VqConsumedElement) * queue_size + sizeof(uint16_t);

    VqDescriptor* descriptor = (VqDescriptor*)0;
    VqAvailable* available = (VqAvailable*)0;
    VqConsumed* used = (VqConsumed*)0;
    uint16_t* free_next = (uint16_t*)kmalloc_aligned(sizeof(uint16_t) * queue_size, 2);
    if (!free_next)
    {
        printf("virtq_init: alloc failed free_next\n");
        return false;
    }

    if (device->version >= 2)
    {
        descriptor = (VqDescriptor*)kmalloc_aligned(descriptor_bytes, 16);
        available = (VqAvailable*)kmalloc_aligned(available_bytes, 2);
        used = (VqConsumed*)kmalloc_aligned(used_bytes, 4);

        if (!descriptor || !available || !used)
        {
            printf("virtq_init: alloc failed descriptor=%x available=%x used=%x\n",
                   (unsigned)(uintptr_t)descriptor,
                   (unsigned)(uintptr_t)available,
                   (unsigned)(uintptr_t)used);
            return false;
        }

        printf("virtq_init: alloc ok descriptor=%x available=%x used=%x\n",
               (unsigned)(uintptr_t)descriptor,
               (unsigned)(uintptr_t)available,
               (unsigned)(uintptr_t)used);

        for (uint16_t i = 0; i < queue_size; i++)
        {
            descriptor[i].address = 0;
            descriptor[i].length = 0;
            descriptor[i].flags = 0;
            descriptor[i].next = 0;
        }

        available->flags = 0;
        available->index = 0;

        used->flags = 0;
        used->index = 0;
    }

    output_queue->device = device;
    output_queue->queue_size = queue_size;
    output_queue->descriptor = descriptor;
    output_queue->available = available;
    output_queue->used = used;
    output_queue->free_next = free_next;
    output_queue->last_used_index = 0;

    virtq_init_free_list(output_queue);

    printf("virtq_init: freelist ok\n");

    mmio_write32(device->base, VIRTIO_MMIO_QUEUE_NUM, queue_size);
    fence_iorw();

    printf("virtq_init: wrote qnum\n");

    if (device->version >= 2)
    {
        mmio_write64_split(device->base, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint64_t)(uintptr_t)descriptor);
        mmio_write64_split(device->base, VIRTIO_MMIO_QUEUE_DRIVER_LOW, (uint64_t)(uintptr_t)available);
        mmio_write64_split(device->base, VIRTIO_MMIO_QUEUE_DEVICE_LOW, (uint64_t)(uintptr_t)used);

        mmio_write32(device->base, VIRTIO_MMIO_QUEUE_READY, 1);
        fence_iorw();

        printf("virtq_init: ready\n");
        return true;
    }

    const uint32_t page_size = 4096u;
    mmio_write32(device->base, VIRTIO_MMIO_GUEST_PAGE_SIZE, page_size);
    mmio_write32(device->base, VIRTIO_MMIO_QUEUE_ALIGN, page_size);
    fence_iorw();

    descriptor_bytes = sizeof(VqDescriptor) * queue_size;
    available_bytes = sizeof(VqAvailable) + sizeof(uint16_t) * (queue_size + 1);
    used_bytes = sizeof(VqConsumed) + sizeof(VqConsumedElement) * queue_size + sizeof(uint16_t);

    const size_t avail_off = descriptor_bytes;
    const size_t used_off = align_up_size(avail_off + available_bytes, page_size);
    const size_t total = align_up_size(used_off + used_bytes, page_size);

    uint8_t* memory = (uint8_t*)kmalloc_aligned(total, page_size);
    if (!memory)
    {
        printf("virtq_init: legacy memory alloc failed\n");
        return false;
    }

    descriptor = (VqDescriptor*)(memory + 0);
    available = (VqAvailable*)(memory + avail_off);
    used = (VqConsumed*)(memory + used_off);

    output_queue->descriptor = descriptor;
    output_queue->available = available;
    output_queue->used = used;

    for (uint16_t i = 0; i < queue_size; i++)
    {
        descriptor[i].address = 0;
        descriptor[i].length = 0;
        descriptor[i].flags = 0;
        descriptor[i].next = 0;
    }
    available->flags = 0;
    available->index = 0;
    used->flags = 0;
    used->index = 0;
    output_queue->last_used_index = 0;

    const uint32_t page_number = ((uint32_t)(uintptr_t)memory) / page_size;
    mmio_write32(device->base, VIRTIO_MMIO_QUEUE_PFN, page_number);
    fence_iorw();

    printf("virtq_init: legacy memory=%x descriptor=%x available=%x used=%x used_off=0x%x page_number=0x%x\n",
           (unsigned)(uintptr_t)memory,
           (unsigned)(uintptr_t)descriptor,
           (unsigned)(uintptr_t)available,
           (unsigned)(uintptr_t)used,
           (unsigned)used_off,
           (unsigned)page_number);

    return true;
}

int virtq_alloc_chain(ViQueue* queue, uint16_t count)
{
    if (!queue || count == 0 || queue->number_free < count) 
    {
        return -1;
    }

    uint16_t head = 0xffffu;
    uint16_t previous = 0xffffu;

    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t index = queue->free_head;
        if (index == 0xffffu)
        {
            return -1;
        }

        queue->free_head = queue->free_next[index];
        queue->free_next[index] = 0xffffu;

        queue->descriptor[index].address = 0;
        queue->descriptor[index].length = 0;
        queue->descriptor[index].flags = 0;
        queue->descriptor[index].next = 0;

        if (head == 0xffffu)
        {
            head = index;
        }
        else
        {
            queue->descriptor[previous].flags |= VIRTQ_DESC_F_NEXT;
            queue->descriptor[previous].next = index;
        }

        previous = index;
    }

    queue->number_free = (uint16_t)(queue->number_free - count);
    return (int)head;
}

void virtq_free_chain(ViQueue* queue, uint16_t head)
{
    if (!queue) 
    {
        return;
    }

    uint16_t current = head;
    while (current != 0xffffu)
    {
        uint16_t next = ((queue->descriptor[current].flags & VIRTQ_DESC_F_NEXT) != 0) ? queue->descriptor[current].next : 0xffffu;

        queue->descriptor[current].flags = 0;
        queue->descriptor[current].next = 0;
        queue->descriptor[current].address = 0;
        queue->descriptor[current].length = 0;

        queue->free_next[current] = queue->free_head;
        queue->free_head = current;
        queue->number_free++;

        current = next;
    }
}

void virtio_mmio_notify_queue(ViMMIODevice* device, uint32_t queue_index)
{
    mmio_write32(device->base, VIRTIO_MMIO_QUEUE_NOTIFY, queue_index);
    fence_iorw();
}

void virtq_submit(ViQueue* queue, uint16_t head)
{
    volatile VqAvailable* available = (volatile VqAvailable*)queue->available;
    uint16_t index = available->index;
    available->ring[index % queue->queue_size] = head;
    fence_iorw();
    available->index = (uint16_t)(index + 1);
    fence_iorw();
}

bool virtq_poll_used(ViQueue* queue, uint16_t* out_id)
{
    volatile VqConsumed* used = (volatile VqConsumed*)queue->used;
    uint16_t used_index = used->index;
    if (queue->last_used_index == used_index)
    {
        return false;
    }

    VqConsumedElement element = used->ring[queue->last_used_index % queue->queue_size];
    queue->last_used_index = (uint16_t)(queue->last_used_index + 1);

    if (out_id) 
    {
        *out_id = (uint16_t)element.id;
    }
    return true;
}

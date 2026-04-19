#include "virtio_gpu.h"
#include "virtio_mmio.h"
#include "memory.h"
#include "utility.h"

#if defined(__GNUC__) && !defined(_MSC_VER)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO      0x0100u
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D    0x0101u
#define VIRTIO_GPU_CMD_RESOURCE_UNREF        0x0102u
#define VIRTIO_GPU_CMD_SET_SCANOUT           0x0103u
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH        0x0104u
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D   0x0105u
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106u
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107u

#define VIRTIO_GPU_RESP_OK_NODATA            0x1100u
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO      0x1101u

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM     2u

typedef struct PACKED
{
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t context_id;
    uint32_t padding;
} VgCommandHeader;

typedef struct PACKED
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} VgRect;

typedef struct PACKED
{
    VgCommandHeader header;
} VgDisplayInfo;

typedef struct PACKED
{
    VgRect rect;
    uint32_t enabled;
    uint32_t flags;
} VgDisplayInstance;

typedef struct PACKED
{
    VgCommandHeader header;
    VgDisplayInstance pmodes[16];
} VgResponseDisplayInfo;

typedef struct PACKED
{
    VgCommandHeader header;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} VgCreateTexture;

typedef struct PACKED
{
    VgCommandHeader header;
    uint32_t resource_id;
    uint32_t entry_count;
} VgAttachBacking;

typedef struct PACKED
{
    uint64_t address;
    uint32_t length;
    uint32_t padding;
} VgMemoryEntry;

typedef struct PACKED
{
    VgCommandHeader header;
    VgRect rect;
    uint32_t scanout_id;
    uint32_t resource_id;
} VgScanoutInfo;

typedef struct PACKED
{
    VgCommandHeader header;
    VgRect rect;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} VgTransferToHost;

typedef struct PACKED
{
    VgCommandHeader header;
    VgRect rect;
    uint32_t resource_id;
    uint32_t padding;
} VgResourceFlush;

typedef struct PACKED
{
    VgCommandHeader header;
} VgResponseHeaderOnly;

static ViMMIODevice _device;
static ViQueue _control_queue;
static FramebufferInfo _framebuffer;
static uint32_t _resource_id = 1;

static inline void fence_iorw(void)
{
    __asm__ volatile ("fence iorw, iorw" : : : "memory");
}

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off)
{
    return *(volatile uint32_t*)(base + off);
}

static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t v)
{
    *(volatile uint32_t*)(base + off) = v;
}

#define VIRTIO_MMIO_STATUS 0x070u

static void zero_bytes(void* p, size_t n)
{
    uint8_t* b = (uint8_t*)p;
    while (n--)
    {
        *b++ = 0;
    }
}

static void gpu_hdr_init(VgCommandHeader* header, uint32_t type)
{
    header->type = type;
    header->flags = 0;
    header->fence_id = 0;
    header->context_id = 0;
    header->padding = 0;
}

static bool gpu_send_cmd(void* request, uint32_t req_len, void* response, uint32_t resp_len)
{
    int head = virtq_alloc_chain(&_control_queue, 2);
    if (head < 0)
    {
        return false;
    }

    uint16_t d0 = (uint16_t)head;
    uint16_t d1 = _control_queue.descriptor[d0].next;

    _control_queue.descriptor[d0].address = (uint64_t)(uintptr_t)request;
    _control_queue.descriptor[d0].length = req_len;

    _control_queue.descriptor[d1].address = (uint64_t)(uintptr_t)response;
    _control_queue.descriptor[d1].length = resp_len;
    _control_queue.descriptor[d1].flags = (uint16_t)(_control_queue.descriptor[d1].flags | VIRTQ_DESC_F_WRITE);

    virtq_submit(&_control_queue, (uint16_t)head);
    virtio_mmio_notify_queue(&_device, 0);

    uint16_t used_id;
    uint32_t spin = 0;
    while (!virtq_poll_used(&_control_queue, &used_id))
    {
        if (++spin == 10000000u)
        {
            printf("virtio-gpu: ctrlq timeout\n");
            virtq_free_chain(&_control_queue, (uint16_t)head);
            return false;
        }
    }

    (void)used_id;
    virtq_free_chain(&_control_queue, (uint16_t)head);
    return true;
}

static bool gpu_get_display(uint32_t* out_w, uint32_t* out_h)
{
    VgDisplayInfo request;
    VgResponseDisplayInfo response;

    gpu_hdr_init(&request.header, VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
    zero_bytes(&response, sizeof(response));

    if (!gpu_send_cmd(&request, sizeof(request), &response, sizeof(response)))
    {
        return false;
    }

    if (response.header.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
    {
        return false;
    }

    if (!response.pmodes[0].enabled)
    {
        return false;
    }

    *out_w = response.pmodes[0].rect.width;
    *out_h = response.pmodes[0].rect.height;
    return true;
}

static bool gpu_create_resource(uint32_t width, uint32_t height)
{
    VgCreateTexture request;
    VgResponseHeaderOnly response;

    gpu_hdr_init(&request.header, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
    request.resource_id = _resource_id;
    request.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    request.width = width;
    request.height = height;

    zero_bytes(&response, sizeof(response));

    if (!gpu_send_cmd(&request, sizeof(request), &response, sizeof(response)))
    {
        return false;
    }

    return response.header.type == VIRTIO_GPU_RESP_OK_NODATA;
}

static bool gpu_attach_backing(void* buffer, uint32_t framebuffer_bytes)
{
    typedef struct PACKED
    {
        VgAttachBacking request;
        VgMemoryEntry entry;
    } VgAttachBackingMessage;

    VgAttachBackingMessage message;
    VgResponseHeaderOnly response;

    gpu_hdr_init(&message.request.header, VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
    message.request.resource_id = _resource_id;
    message.request.entry_count = 1;

    message.entry.address = (uint64_t)(uintptr_t)buffer;
    message.entry.length = framebuffer_bytes;
    message.entry.padding = 0;

    zero_bytes(&response, sizeof(response));

    if (!gpu_send_cmd(&message, sizeof(message), &response, sizeof(response)))
    {
        return false;
    }

    return response.header.type == VIRTIO_GPU_RESP_OK_NODATA;
}

static bool gpu_set_scanout(uint32_t width, uint32_t height)
{
    VgScanoutInfo request;
    VgResponseHeaderOnly response;

    gpu_hdr_init(&request.header, VIRTIO_GPU_CMD_SET_SCANOUT);
    request.rect.x = 0;
    request.rect.y = 0;
    request.rect.width = width;
    request.rect.height = height;
    request.scanout_id = 0;
    request.resource_id = _resource_id;

    zero_bytes(&response, sizeof(response));

    if (!gpu_send_cmd(&request, sizeof(request), &response, sizeof(response)))
    {
        return false;
    }

    if (response.header.type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        printf("virtio-gpu: set_scanout response=0x%x\n", (unsigned)response.header.type);
        return false;
    }

    return true;
}

bool virtio_gpu_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!_framebuffer.buffer || w == 0 || h == 0) 
    {
        return false;
    }

    if (x >= _framebuffer.width || y >= _framebuffer.height) 
    {
        return false;
    }

    if (x + w > _framebuffer.width) 
    {
        w = _framebuffer.width - x;
    }

    if (y + h > _framebuffer.height)
    {
        h = _framebuffer.height - y;
    }

    VgTransferToHost transfer;
    VgResourceFlush flush;
    VgResponseHeaderOnly response;

    gpu_hdr_init(&transfer.header, VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
    transfer.rect.x = x;
    transfer.rect.y = y;
    transfer.rect.width = w;
    transfer.rect.height = h;
    transfer.offset = (uint64_t)y * (uint64_t)_framebuffer.stride_bytes + (uint64_t)x * 4ull;
    transfer.resource_id = _resource_id;
    transfer.padding = 0;

    zero_bytes(&response, sizeof(response));
    if (!gpu_send_cmd(&transfer, sizeof(transfer), &response, sizeof(response)))
    {
        return false;
    }
    if (response.header.type != VIRTIO_GPU_RESP_OK_NODATA) 
    {
        return false;
    }

    gpu_hdr_init(&flush.header, VIRTIO_GPU_CMD_RESOURCE_FLUSH);
    flush.rect.x = x;
    flush.rect.y = y;
    flush.rect.width = w;
    flush.rect.height = h;
    flush.resource_id = _resource_id;
    flush.padding = 0;

    zero_bytes(&response, sizeof(response));
    if (!gpu_send_cmd(&flush, sizeof(flush), &response, sizeof(response)))
    {
        return false;
    }

    return response.header.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool virtio_gpu_init(FramebufferInfo* out_fb)
{
    printf("virtio-gpu: init...\n");
    if (!virtio_mmio_find_device(16, &_device))
    {
        printf("virtio-gpu: not found\n");
        return false;
    }

    printf("virtio-gpu: found @0x%x v%u\n", (unsigned)_device.base, (unsigned)_device.version);

    if (!virtio_mmio_init(&_device))
    {
        printf("virtio-gpu: init failed\n");
        return false;
    }

    printf("virtio-gpu: mmio init ok\n");

    uint64_t accepted;
    (void)accepted;

    if (!virtio_mmio_negotiate(&_device, (1ull << 32), &accepted))
    {
        printf("virtio-gpu: feature negotiation failed\n");
        return false;
    }

    printf("virtio-gpu: features ok (accepted hi=0x%x)\n", (unsigned)(accepted >> 32));

    if (!virtq_init(&_device, 0, 16, &_control_queue))
    {
        printf("virtio-gpu: ctrlq init failed (need virtio-mmio v2)\n");
        return false;
    }

    printf("virtio-gpu: ctrlq ready\n");

    uint32_t status = mmio_read32(_device.base, VIRTIO_MMIO_STATUS);
    // driver status OK
    mmio_write32(_device.base, VIRTIO_MMIO_STATUS, status | 4u);
    fence_iorw();
    printf("virtio-gpu: driver_ok\n");

    uint32_t w = 0, h = 0;
    if (!gpu_get_display(&w, &h))
    {
        printf("virtio-gpu: GET_DISPLAY_INFO failed\n");
        return false;
    }

    printf("virtio-gpu: display %dx%d\n", (int)w, (int)h);

    const uint32_t bits_per_pixel = 4;
    const uint32_t stride = w * bits_per_pixel;
    const uint32_t framebuffer_bytes = stride * h;

    uint32_t* buffer = (uint32_t*)kmalloc_aligned(framebuffer_bytes, 4096);
    if (!buffer)
    {
        printf("virtio-gpu: framebuffer alloc failed\n");
        return false;
    }

    zero_bytes(buffer, framebuffer_bytes);

    if (!gpu_create_resource(w, h))
    {
        printf("virtio-gpu: create resource failed\n");
        return false;
    }

    if (!gpu_attach_backing(buffer, framebuffer_bytes))
    {
        printf("virtio-gpu: attach backing failed\n");
        return false;
    }

    if (!gpu_set_scanout(w, h))
    {
        printf("virtio-gpu: set scanout failed\n");
        return false;
    }

    _framebuffer.buffer = buffer;
    _framebuffer.width = w;
    _framebuffer.height = h;
    _framebuffer.stride_bytes = stride;

    if (out_fb) 
    {
        *out_fb = _framebuffer;
    }

    virtio_gpu_flush_rect(0, 0, w, h);

    printf("virtio-gpu: %dx%d framebuffer ready\n", (int)w, (int)h);
    return true;
}

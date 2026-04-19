#ifndef STRATUS_MMIO_H
#define STRATUS_MMIO_H

// Stratus: mmio.h
// (c) 2026 Connor J. Link. All Rights Reserved.

// kernel controls all of high memory
#define KERNEL_BASE 0x80000000u

// R4G4B4A4, 16bpp
#define COLOR_DEPTH 4u

#define TEXTURE_BASE 0xD0000000u
#define TEXTURE_WIDTH 256u
#define TEXTURE_HEIGHT 256u
#define TEXTURE_SIZE (TEXTURE_WIDTH * TEXTURE_HEIGHT * COLOR_DEPTH)
#define TEXTURE_SLOTS 16u
#define TEXTURE_ADDRESS(slot) (TEXTURE_BASE + (slot) * TEXTURE_SIZE)

#define FRAMEBUFFER_BASE 0xE0000000u
#define FRAMEBUFFER_WIDTH 640u
#define FRAMEBUFFER_HEIGHT 480u
#define FRAMEBUFFER_SIZE (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * COLOR_DEPTH)
// three application framebuffers, plus the master composite
#define FRAMEBUFFER_SLOTS (3u + 1u)
#define FRAMEBUFFER_ADDRESS(slot) (FRAMEBUFFER_BASE + (slot) * FRAMEBUFFER_SIZE)


#define MMIO_BASE 0xF0000000u

// no arguments, any write to the address will trigger a request
#define MMIO_REQUEST_FRAMEBUFFER (MMIO_BASE + 0x0000u)
// spinloop on this to get an address
#define MMIO_FRAMEBUFFER_BUSY (MMIO_BASE + 0x0004u)
// read the index here (either 1, 2, 3, or -1 for failure; never 0 as it's reserved)
#define MMIO_FRAMEBUFFER_RESPONSE (MMIO_BASE + 0x0008u)


// no arguments, any write to the address will trigger a request
#define MMIO_REQUEST_TEXTURE (MMIO_BASE + 0x0020u)
// spinloop on this to get an address
#define MMIO_TEXTURE_BUSY (MMIO_BASE + 0x0024u)
// read the index here (either 0 through 15, or -1 for failure)
#define MMIO_TEXTURE_RESPONSE (MMIO_BASE + 0x0028u)


/* used for bitmap blitting */
// values beyond 10 bits are reserved
#define MMIO_SET_RECT_X (MMIO_BASE + 0x0030u)
#define MMIO_SET_RECT_Y (MMIO_BASE + 0x0034u)
#define MMIO_SET_RECT_WIDTH (MMIO_BASE + 0x0038u)
#define MMIO_SET_RECT_HEIGHT (MMIO_BASE + 0x003Cu)


/* selector commands */
#define MMIO_SET_FRAMEBUFFER_SLOT (MMIO_BASE + 0x0040u)
#define MMIO_SET_TEXTURE_SLOT (MMIO_BASE + 0x0044u)
#define MMIO_SET_COLOR (MMIO_BASE + 0x0048u)


/* texturing commands */
#define MMIO_SET_TEXTURE_X (MMIO_BASE + 0x0050u)
#define MMIO_SET_TEXTURE_Y (MMIO_BASE + 0x0054u)
#define MMIO_SET_TEXTURE_WIDTH (MMIO_BASE + 0x0058u)
#define MMIO_SET_TEXTURE_HEIGHT (MMIO_BASE + 0x005Cu)


/* blitting commands */
// no arguments, any write to the address will trigger a request
#define MMIO_BLIT_SOLID_COLOR (MMIO_BASE + 0x0060u)
// no arguments, any write to the address will trigger a request
#define MMIO_BLIT_TEXTURE (MMIO_BASE + 0x0064u)
// spinloop on this to synchronously wait for render completion
#define MMIO_BLIT_BUSY (MMIO_BASE + 0x0068u)


/* compositing commands */
// write the framebuffer slot (1, 2, 3) to composite atop the master framebuffer
#define MMIO_REQUEST_COMPOSITE (MMIO_BASE + 0x0070u)
// spinloop on this to synchronously wait for composite completion
#define MMIO_COMPOSITE_BUSY (MMIO_BASE + 0x0074u)



#define MMIO_PRESENT_FRAMEBUFFER (MMIO_BASE + 0x1000u)

#define USER_BASE 0x00000000u


/* helpful actions */

#define MMIO_CLEAR_FRAMEBUFFER() do { \
    *((volatile uint32_t*)(MMIO_SET_RECT_X)) = 0; \
    *((volatile uint32_t*)(MMIO_SET_RECT_Y)) = 0; \
    *((volatile uint32_t*)(MMIO_SET_RECT_WIDTH)) = FRAMEBUFFER_WIDTH; \
    *((volatile uint32_t*)(MMIO_SET_RECT_HEIGHT)) = FRAMEBUFFER_HEIGHT; \
    *((volatile uint32_t*)(MMIO_SET_COLOR)) = 0; /* black, no alpha */ \
    *((volatile uint32_t*)(MMIO_BLIT_SOLID_COLOR)) = 0; \
    while (*((volatile uint32_t*)(MMIO_BLIT_BUSY))) {} \
} while (0)

#define MMIO_BLIT_TEXTURE_RECT(texture_slot, x, y, width, height) do { \
    *((volatile uint32_t*)(MMIO_SET_TEXTURE_SLOT)) = (texture_slot); \
    *((volatile uint32_t*)(MMIO_SET_TEXTURE_X)) = (x); \
    *((volatile uint32_t*)(MMIO_SET_TEXTURE_Y)) = (y); \
    *((volatile uint32_t*)(MMIO_SET_TEXTURE_WIDTH)) = (width); \
    *((volatile uint32_t*)(MMIO_SET_TEXTURE_HEIGHT)) = (height); \
    *((volatile uint32_t*)(MMIO_BLIT_TEXTURE)) = 0; \
    while (*((volatile uint32_t*)(MMIO_BLIT_BUSY))) {} \
} while (0)

#define MMIO_COMPOSITE_FRAMEBUFFER(framebuffer_slot) do { \
    *((volatile uint32_t*)(MMIO_SET_FRAMEBUFFER_SLOT)) = (framebuffer_slot); \
    *((volatile uint32_t*)(MMIO_REQUEST_COMPOSITE)) = 0; \
    while (*((volatile uint32_t*)(MMIO_COMPOSITE_BUSY))) {} \
} while (0)

#define MMIO_COMPOSITE_FRAMEBUFFERS(...) do { \
    uint32_t slots[] = { __VA_ARGS__ }; \
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) { \
        MMIO_COMPOSITE_FRAMEBUFFER(slots[i]); \
    } \
} while (0)

#endif

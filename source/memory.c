#include "memory.h"

#include "utility.h"

extern char __bss_end[];
extern char __stack_top[];

static uintptr_t heap_ptr;
static uintptr_t heap_limit;

static inline uintptr_t align_up_uintptr(uintptr_t v, uintptr_t align)
{
    if (align == 0) return v;
    return (v + (align - 1)) & ~(align - 1);
}

void memory_init(void)
{
    if (heap_ptr != 0)
    {
        return;
    }

    printf("mem: __bss_end=0x%x __stack_top=0x%x\n",
           (unsigned)(uintptr_t)__bss_end,
           (unsigned)(uintptr_t)__stack_top);

    heap_ptr = align_up_uintptr((uintptr_t)__bss_end, 16);

    // maintain stack guard
    const uintptr_t stack_top = (uintptr_t)__stack_top;
    heap_limit = stack_top - (64u * 1024u);
}

void* kmalloc_aligned(size_t size, size_t align)
{
    if (size == 0)
    {
        return (void*)0;
    }

    uintptr_t p = align_up_uintptr(heap_ptr, (align == 0) ? 1u : (uintptr_t)align);

    if (p + size > heap_limit)
    {
        return (void*)0;
    }

    heap_ptr = p + size;
    return (void*)p;
}

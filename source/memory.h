#ifndef STRATUS_MEMORY_H
#define STRATUS_MEMORY_H

#include <stddef.h>
#include <stdint.h>

// Stratus memory.h
// (c) Connor J. Link. All Rights Reserved.

void memory_init(void);
void* kmalloc_aligned(size_t size, size_t align);

#endif

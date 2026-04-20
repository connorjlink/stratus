#ifndef STRATUS_UTILITY_H
#define STRATUS_UTILITY_H

#include <stddef.h>

// Stratus: utility.h
// (c) Connor J. Link. All Rights Reserved.

size_t max(size_t x, size_t y);
size_t min(size_t x, size_t y);

size_t strlen(const char* str);
char* strcpy(char* out, const char* str);
int strcmp(const char* string1, const char* string2);
void* memset(void* pointer, unsigned char value, size_t number);
void* memcpy(void* destination, const void* source, size_t number);

void putchar(uint8_t ascii);
void printf(const char* format, ...);

#error TODO: figure out how to get rid of GNU assembly and statement expressions

#define READ_CSR(csr)                     \
({                                        \
    uint32_t _value;                      \
    __asm__ volatile ("csrr %0, " #csr    \
                      : "=r"(_value));    \
    _value;                               \
})

#define write_csr(csr, value)             \
({                                        \
    uint32_t _v = (uint32_t)(value);      \
    __asm__ volatile ("csrw " #csr ", %0" \
                      :                   \
                      : "rK"(_v));        \
})

#endif

#ifndef STRATUS_UTILITY_H
#define STRATUS_UTILITY_H

// Stratus: utility.h
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stddef.h>

size_t max(size_t x, size_t y);
size_t min(size_t x, size_t y);

size_t strlen(const char* str);
char* strcpy(char* out, const char* str);
int strcmp(const char* string1, const char* string2);
void* memset(void* pointer, unsigned char value, size_t number);
void* memcpy(void* destination, const void* source, size_t number);

void putchar(char c);
void printf(const char* format, ...);

#endif

#ifndef STRATUS_FB_CONSOLE_H
#define STRATUS_FB_CONSOLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Stratus console.h
// (c) Connor J. Link. All Rights Reserved.

#define GLYPH_W 8u
#define GLYPH_H 16u

void terminal_initialize(void);
void terminal_putentryat(uint8_t ascii, Palette color, size_t x, size_t y);
void terminal_putchar(uint8_t ascii, size_t* x, size_t* y);
void terminal_write(const char* data, size_t size, size_t x, size_t y);
void terminal_writestring(const char* data, size_t x, size_t y);
bool terminal_getentryat(size_t x, size_t y, char* out_c, uint8_t* out_color);
void terminal_flush(void);

extern const size_t _columns;
extern const size_t _rows;

#endif

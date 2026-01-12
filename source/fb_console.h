#ifndef STRATUS_FB_CONSOLE_H
#define STRATUS_FB_CONSOLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void terminal_initialize(void);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_putchar(char c, size_t* x, size_t* y);
void terminal_write(const char* data, size_t size, size_t x, size_t y);
void terminal_writestring(const char* data, size_t x, size_t y);
void terminal_get_size(size_t* out_cols, size_t* out_rows);
bool terminal_getentryat(size_t x, size_t y, char* out_c, uint8_t* out_color);
void terminal_flush(void);

#endif

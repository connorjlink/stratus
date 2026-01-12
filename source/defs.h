#ifndef STRATUS_DEFS_H
#define STRATUS_DEFS_H

// Stratus: defs.h
// (c) 2026 Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
} VGAColor;

typedef struct
{
    VGAColor fg, bg;
} VGAPalette;
#define PALETTE(fg, bg) (VGAPalette){ fg, bg }
#define INVERT_PALETTE(palette) (VGAPalette){ palette.bg, palette.fg }

typedef struct
{
    char c;
    size_t x, y;
    VGAPalette palette;
} VGACharacter;
#define CHARACTER(c, x, y, palette) (VGACharacter){ c, x, y, palette }


typedef struct
{
    size_t x, y;
} Point;
#define POINT(x, y) (Point){ x, y }

typedef struct
{
    Point pos, size;
} Rect;
#define RECT(pos, size) (Rect){ pos, size }

extern Rect _explorer_rect;
extern Rect _console_rect;
extern Rect _navigator_rect;

extern Point _console_cursor;

extern uint8_t _active_color;

#endif

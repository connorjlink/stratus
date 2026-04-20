#ifndef STRATUS_DEFS_H
#define STRATUS_DEFS_H

// Stratus: defs.h
// (c) Connor J. Link. All Rights Reserved.

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN = 6,
    COLOR_LIGHT_GREY = 7,
    COLOR_DARK_GREY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_LIGHT_BROWN = 14,
    COLOR_WHITE = 15,
} Color;

typedef struct
{
    Color foreground : 4, background : 4;
} Palette;
#define PALETTE(fg, bg) (Palette){ fg, bg }
#define INVERT_PALETTE(palette) (Palette){ palette.background, palette.foreground }

typedef struct
{
    uint8_t ascii;
    size_t x, y;
    Palette palette;
} Character;
#define CHARACTER(ascii, x, y, palette) (Character){ ascii, x, y, palette }


typedef struct
{
    size_t x, y;
} Point;
#define POINT(x, y) (Point){ x, y }

typedef struct
{
    Point position, size;
} Rectangle;
#define Rectangle(position, size) (Rectangle){ position, size }

extern Rectangle _explorer_rect;
extern Rectangle _console_rect;
extern Rectangle _navigator_rect;

extern Point _console_cursor;

extern Palette _active_palette;

void layout_init(size_t columns, size_t rows);

#endif

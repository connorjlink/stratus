#include "defs.h"

// Stratus: defs.c
// (c) Connor J. Link. All Rights Reserved.

Rectangle _explorer_rect = { { 0, 1 }, { 20, 22 } };
Rectangle _console_rect = { { 21, 15 }, { 58, 8 } };
Rectangle _navigator_rect = { { 21, 1 }, { 58, 13 } };

Point _console_cursor = { 0, 0 };

Palette _active_palette = { .foreground = COLOR_LIGHT_GREY, .background = COLOR_BLUE };

void layout_init(size_t columns, size_t rows)
{
    if (columns < 40 || rows < 15)
    {
        return;
    }

    const size_t content_h = rows - 2;

    size_t explorer_delta = columns / 4;
    if (explorer_delta < 20) 
    {
        explorer_delta = 20;
    }
    if (explorer_delta > columns - 22) 
    {
        explorer_delta = columns - 22;
    }

    const size_t right_left = explorer_delta + 1;
    const size_t right_delta = (columns - 1) - right_left;

    size_t console_h = content_h / 3;
    if (console_h < 9) 
    {
        console_h = 9;
    }
    if (console_h > content_h - 6) 
    {
        console_h = content_h - 6;
    }

    const size_t navigator_h = content_h - console_h;

    _explorer_rect = (Rectangle){ { 0, 1 }, { explorer_delta, content_h - 1 } };
    _navigator_rect = (Rectangle){ { right_left, 1 }, { right_delta, navigator_h - 1 } };
    _console_rect = (Rectangle){ { right_left, 1 + navigator_h }, { right_delta, console_h - 1 } };
}


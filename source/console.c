// Stratus console.c
// (c) Connor J. Link. All Rights Reserved.

#include "defs.h"
#include "utility.h"
#include "memory.h"
#include "console.h"
#include "mmio.h"

typedef int8_t FramebufferSlot;
typedef uint32_t Address;

const size_t _columns = FRAMEBUFFER_WIDTH / GLYPH_W;
const size_t _rows = FRAMEBUFFER_HEIGHT / GLYPH_H;

static Cell* _cells;

static bool _dirty;
static uint32_t _dirty_x0, _dirty_y0, _dirty_x1, _dirty_y1;

static inline Cell* cell_at(size_t x, size_t y)
{
    return &_cells[y * _columns + x];
}

static const uint16_t _rgba[16] =
{
    0x000Fu, // black
    0x00AFu, // blue
    0x0A0Fu, // green
    0x0AAFu, // cyan
    0xA00Fu, // red
    0xA0AFu, // magenta
    0xA50Fu, // brown
    0xAAAFu, // light grey
    0x555Fu, // dark grey
    0x55FFu, // light blue
    0x5F5Fu, // light green
    0x5FFFu, // light cyan
    0xF55Fu, // light red
    0xF5FFu, // light magenta
    0xFF5Fu, // light brown
    0xFFFFu, // white
};

static inline uint32_t foreground_from_palette(Palette palette)
{
    return _rgba[palette.foreground];
}

static inline uint32_t background_from_palette(Palette palette)
{
    return _rgba[palette.background];
}

static inline void mark_dirty_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!_dirty)
    {
        _dirty = true;
        _dirty_x0 = x;
        _dirty_y0 = y;
        _dirty_x1 = x + w;
        _dirty_y1 = y + h;
        return;
    }

    if (x < _dirty_x0)
    {
        _dirty_x0 = x;
    }
    if (y < _dirty_y0)
    {
        _dirty_y0 = y;
    }
    if (x + w > _dirty_x1)
    {
        _dirty_x1 = x + w;
    }
    if (y + h > _dirty_y1) 
    {
        _dirty_y1 = y + h;
    }
}

static inline void put_pixel(FramebufferSlot slot, uint16_t x, uint16_t y, uint16_t rgba)
{
    const uint32_t address = FRAMEBUFFER_PIXEL_ADDRESS(slot, x, y);
    MMIO16(address) = rgba;
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgba)
{
    if (x >= FRAMEBUFFER_WIDTH || y >= FRAMEBUFFER_HEIGHT) 
    {
        return;
    }

    if (x + w > FRAMEBUFFER_WIDTH) 
    {
        w = FRAMEBUFFER_WIDTH - x;
    }
    if (y + h > FRAMEBUFFER_HEIGHT)
    {
        h = FRAMEBUFFER_HEIGHT - y;
    }

    uint32_t stride_pixels = _framebuffer.stride_bytes / 4u;

    for (uint32_t yy = 0; yy < h; yy++)
    {
        uint32_t* row = &_framebuffer.buffer[(y + yy) * stride_pixels + x];

        for (uint32_t xx = 0; xx < w; xx++)
        {
            row[xx] = rgba;
        }
    }

    mark_dirty_rect(x, y, w, h);
}

typedef struct 
{ 
    uint8_t ascii; 
    uint8_t rows[7];
} Glyph5x7;

static const Glyph5x7 _glyphs_5x7[] =
{
    // digits */
    { '0', { 0x1E, 0x21, 0x23, 0x25, 0x29, 0x31, 0x1E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x1E, 0x21, 0x01, 0x06, 0x18, 0x20, 0x3F } },
    { '3', { 0x1E, 0x21, 0x01, 0x0E, 0x01, 0x21, 0x1E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x3F, 0x02, 0x02 } },
    { '5', { 0x3F, 0x20, 0x3E, 0x01, 0x01, 0x21, 0x1E } },
    { '6', { 0x0E, 0x10, 0x20, 0x3E, 0x21, 0x21, 0x1E } },
    { '7', { 0x3F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
    { '8', { 0x1E, 0x21, 0x21, 0x1E, 0x21, 0x21, 0x1E } },
    { '9', { 0x1E, 0x21, 0x21, 0x1F, 0x01, 0x02, 0x1C } },

    // uppercase letters
    { 'A', { 0x0E, 0x11, 0x21, 0x21, 0x3F, 0x21, 0x21 } },
    { 'B', { 0x3E, 0x21, 0x21, 0x3E, 0x21, 0x21, 0x3E } },
    { 'C', { 0x1E, 0x21, 0x20, 0x20, 0x20, 0x21, 0x1E } },
    { 'D', { 0x3C, 0x22, 0x21, 0x21, 0x21, 0x22, 0x3C } },
    { 'E', { 0x3F, 0x20, 0x20, 0x3E, 0x20, 0x20, 0x3F } },
    { 'F', { 0x3F, 0x20, 0x20, 0x3E, 0x20, 0x20, 0x20 } },
    { 'G', { 0x1E, 0x21, 0x20, 0x27, 0x21, 0x21, 0x1E } },
    { 'H', { 0x21, 0x21, 0x21, 0x3F, 0x21, 0x21, 0x21 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { 'J', { 0x07, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1C } },
    { 'K', { 0x21, 0x22, 0x24, 0x38, 0x24, 0x22, 0x21 } },
    { 'L', { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3F } },
    { 'M', { 0x21, 0x33, 0x2D, 0x21, 0x21, 0x21, 0x21 } },
    { 'N', { 0x21, 0x31, 0x29, 0x25, 0x23, 0x21, 0x21 } },
    { 'O', { 0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E } },
    { 'P', { 0x3E, 0x21, 0x21, 0x3E, 0x20, 0x20, 0x20 } },
    { 'Q', { 0x1E, 0x21, 0x21, 0x21, 0x25, 0x22, 0x1D } },
    { 'R', { 0x3E, 0x21, 0x21, 0x3E, 0x24, 0x22, 0x21 } },
    { 'S', { 0x1F, 0x20, 0x20, 0x1E, 0x01, 0x01, 0x3E } },
    { 'T', { 0x3F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E } },
    { 'V', { 0x21, 0x21, 0x21, 0x21, 0x21, 0x12, 0x0C } },
    { 'W', { 0x21, 0x21, 0x21, 0x21, 0x2D, 0x33, 0x21 } },
    { 'X', { 0x21, 0x12, 0x0C, 0x0C, 0x0C, 0x12, 0x21 } },
    { 'Y', { 0x21, 0x12, 0x0C, 0x04, 0x04, 0x04, 0x04 } },
    { 'Z', { 0x3F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x3F } },

    // lowercase letters (6x7, shifted-left variants of common 5x7 shapes)
    { 'a', { 0x00, 0x00, 0x1C, 0x02, 0x1E, 0x22, 0x1E } },
    { 'b', { 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x3C } },
    { 'c', { 0x00, 0x00, 0x1C, 0x20, 0x20, 0x20, 0x1C } },
    { 'd', { 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1E } },
    { 'e', { 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x20, 0x1C } },
    { 'f', { 0x0C, 0x10, 0x3C, 0x10, 0x10, 0x10, 0x10 } },
    { 'g', { 0x00, 0x00, 0x1E, 0x22, 0x1E, 0x02, 0x1C } },
    { 'h', { 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22 } },
    { 'i', { 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1C } },
    { 'j', { 0x04, 0x00, 0x0C, 0x04, 0x04, 0x24, 0x18 } },
    { 'k', { 0x20, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22 } },
    { 'l', { 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C } },
    { 'm', { 0x00, 0x00, 0x34, 0x2A, 0x2A, 0x2A, 0x2A } },
    { 'n', { 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22 } },
    { 'o', { 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C } },
    { 'p', { 0x00, 0x00, 0x3C, 0x22, 0x3C, 0x20, 0x20 } },
    { 'q', { 0x00, 0x00, 0x1E, 0x22, 0x1E, 0x02, 0x02 } },
    { 'r', { 0x00, 0x00, 0x2C, 0x30, 0x20, 0x20, 0x20 } },
    { 's', { 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C } },
    { 't', { 0x10, 0x3C, 0x10, 0x10, 0x10, 0x10, 0x0C } },
    { 'u', { 0x00, 0x00, 0x22, 0x22, 0x22, 0x26, 0x1A } },
    { 'v', { 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08 } },
    { 'w', { 0x00, 0x00, 0x22, 0x2A, 0x2A, 0x2A, 0x14 } },
    { 'x', { 0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22 } },
    { 'y', { 0x00, 0x00, 0x22, 0x22, 0x1E, 0x02, 0x1C } },
    { 'z', { 0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E } },

    // symbols
    { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C } },
    { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } },
    { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } },
    { ';', { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x10 } },
    { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
    { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
    { '/', { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 } },
    { '\\',{ 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00 } },
    { ',', { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08 } },
    { '\'',{ 0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 } },
    { '"', { 0x0A, 0x0A, 0x04, 0x00, 0x00, 0x00, 0x00 } },
    { '?', { 0x1E, 0x21, 0x01, 0x06, 0x04, 0x00, 0x04 } },
    { '<', { 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04 } },
    { '>', { 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10 } },
    { '[', { 0x3C, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3C } },
    { ']', { 0x3C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3C } },
    { '{', { 0x1C, 0x10, 0x10, 0x20, 0x10, 0x10, 0x1C } },
    { '}', { 0x38, 0x08, 0x08, 0x04, 0x08, 0x08, 0x38 } },
    { '+', { 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 } },
    { '=', { 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00 } },
    { '_', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E } },
    { '@', { 0x1C, 0x22, 0x2E, 0x2A, 0x2E, 0x20, 0x1C } },
    { '#', { 0x14, 0x3E, 0x14, 0x14, 0x3E, 0x14, 0x00 } },
    { '$', { 0x08, 0x1E, 0x28, 0x1C, 0x0A, 0x3C, 0x08 } },
    { '%', { 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26 } },
    { '&', { 0x18, 0x24, 0x28, 0x10, 0x2A, 0x24, 0x1A } },
    { '*', { 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00 } },
    { '|', { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 } },
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
};

static bool get_5x7(uint8_t ascii, uint8_t out_rows[7])
{
    const size_t glyph_count = sizeof(_glyphs_5x7) / sizeof(_glyphs_5x7[0]);

    for (size_t i = 0; i < glyph_count; i++)
    {
        if (_glyphs_5x7[i].ascii == ascii)
        {
            for (size_t j = 0; j < 7; j++)
            {
                out_rows[j] = _glyphs_5x7[i].rows[j];
            }

            return true;
        }
    }

    // fallback to use uppercase if lowercase glyph not present
    if (ascii >= 'a' && ascii <= 'z')
    {
        char upper = (char)(ascii - 'a' + 'A');
        for (size_t i = 0; i < glyph_count; i++)
        {
            if (_glyphs_5x7[i].ascii == upper)
            {
                for (size_t j = 0; j < 7; j++)
                {
                    out_rows[j] = _glyphs_5x7[i].rows[j];
                }
                return true;
            }
        }
    }

    return false;
}

static void draw_box_char(FramebufferSlot slot, uint8_t ascii, uint32_t x, uint32_t y, uint32_t foreground, uint32_t background)
{
    fill_rect(x, y, GLYPH_W, GLYPH_H, background);

    const uint32_t x_midpoint = x + (GLYPH_W / 2);
    const uint32_t y_midpoint = y + (GLYPH_H / 2);

    const uint32_t x0 = x;
    const uint32_t x1 = x + GLYPH_W - 1;
    const uint32_t y0 = y;
    const uint32_t y1 = y + GLYPH_H - 1;

    switch (ascii)
    {
        case 0xC4: // horizontal
            for (uint32_t x = x0; x <= x1; x++)
            {
                put_pixel(slot, x, y_midpoint, foreground);
            }
            break;

        case 0xB3: // vertical
            for (uint32_t y = y0; y <= y1; y++)
            {
                put_pixel(slot, x_midpoint, y, foreground);
            }
            break;

        case 0xDA: // top-left
            for (uint32_t x = x_midpoint; x <= x1; x++)
            {
                put_pixel(slot, x, y_midpoint, foreground);
            }
            for (uint32_t y = y_midpoint; y <= y1; y++)
            {
                put_pixel(slot, x_midpoint, y, foreground);
            }
            break;

        case 0xBF: // top-right
            for (uint32_t x = x0; x <= x_midpoint; x++)
            {
                put_pixel(slot, x, y_midpoint, foreground);
            }
            for (uint32_t y = y_midpoint; y <= y1; y++)
            {
                put_pixel(slot, x_midpoint, y, foreground);
            }
            break;

        case 0xC0: // bottom-left
            for (uint32_t x = x_midpoint; x <= x1; x++)
            {
                put_pixel(slot, x, y_midpoint, foreground);
            }
            for (uint32_t y = y0; y <= y_midpoint; y++)
            {
                put_pixel(slot, x_midpoint, y, foreground);
            }
            break;
             
        case 0xD9: // bottom-right
            for (uint32_t x = x0; x <= x_midpoint; x++)
            {
                put_pixel(slot, x, y_midpoint, foreground);
            }
            for (uint32_t y = y0; y <= y_midpoint; y++)
            {
                put_pixel(slot, x_midpoint, y, foreground);
            }
            break;

        default:
            break;
    }

    mark_dirty_rect(x, y, GLYPH_W, GLYPH_H);
}

static void draw_glyph(FramebufferSlot slot, uint8_t ascii, Palette palette, uint32_t x, uint32_t y)
{
    const uint16_t foreground = foreground_from_palette(palette);
    const uint16_t background = background_from_palette(palette);

    const uint32_t x = x * GLYPH_W;
    const uint32_t y = y * GLYPH_H;

    if ((uint8_t)ascii >= 0x80)
    {
        draw_box_char(slot, ascii, x, y, foreground, background);
        return;
    }

    fill_rect(x, y, GLYPH_W, GLYPH_H, background);

    uint8_t rows[7];
    if (!get_5x7(ascii, rows))
    {
        (void)get_5x7('?', rows);
    }

    // glyph table uses 6-bit rows rendering as 6x7
    // center 6x7 inside 8x16: x offset 1, y offset 4
    const uint32_t x0 = x + 1;
    const uint32_t y0 = y + 4;

    for (uint32_t r = 0; r < 7; r++)
    {
        uint8_t bits = rows[r];
        for (uint32_t column = 0; column < 6; column++)
        {
            if (bits & (1u << (5 - column)))
            {
                put_pixel(slot, x0 + column, y0 + r, foreground);
            }
        }
    }

    mark_dirty_rect(x, y, GLYPH_W, GLYPH_H);
}

void terminal_initialize(void)
{
    memory_init();

    _dirty = false;

    _cells = (Cell*)kmalloc_aligned(sizeof(Cell) * _columns * _rows, 16);
    if (!_cells)
    {
        printf("fb_console: cell alloc failed\n");
        return;
    }

    fill_rect(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, background_from_palette(_active_palette));

    for (size_t y = 0; y < _rows; y++)
    {
        for (size_t x = 0; x < _columns; x++)
        {
            Cell* cell = cell_at(x, y);
            cell->ascii = ' ';
            cell->color = _active_palette;
        }
    }

    terminal_flush();
}

void terminal_putentryat(FramebufferSlot slot, uint8_t ascii, uint8_t color, size_t x, size_t y)
{
    if (x >= _columns || y >= _rows) 
    {
        return;
    }

    Cell* cell = cell_at(x, y);
    cell->ascii = ascii;
    cell->color = color;

    draw_glyph(slot, ascii, color, (uint32_t)x, (uint32_t)y);
}

void terminal_putchar(FramebufferSlot slot, uint8_t ascii, size_t* x, size_t* y)
{
    if (!x || !y) 
    {
        return;
    }

    switch (c)
    {
        case '\n':
            *x = 0;
            (*y)++;
            return;

        case '\r':
            *x = 0;
            return;

        case '\0':
            return;
    }

    terminal_putentryat(slot, ascii, _active_palette, *x, *y);

    (*x)++;

    if ((*x) == _columns)
    {
        *x = 0;
        (*y)++;

        if ((*y) == _rows)
        {
            *y = 0;
        }
    }
}

void terminal_write(FramebufferSlot slot, const char* data, size_t size, size_t x, size_t y)
{
    for (size_t i = 0; i < size; i++)
    {
        terminal_putchar(slot, data[i], &x, &y);
    }
}

void terminal_writestring(FramebufferSlot slot, const char* data, size_t x, size_t y)
{
    terminal_write(slot, data, strlen(data), x, y);
}

bool terminal_getentryat(size_t x, size_t y, char* out_c, uint8_t* out_color)
{
    if (x >= _columns || y >= _rows)
    {
        return false;
    }

    Cell* cell = cell_at(x, y);
    if (out_c)
    {
        *out_c = cell->ascii;
    }
    if (out_color)
    {
        *out_color = cell->color;
    }
    return true;
}

void terminal_flush(void)
{
    if (!_dirty)
    {
        return;
    }

    _dirty = false;

    if (_dirty_x1 <= _dirty_x0 || _dirty_y1 <= _dirty_y0) 
    {
        return;
    }

    invalidate_rect(_dirty_x0, _dirty_y0, _dirty_x1 - _dirty_x0, _dirty_y1 - _dirty_y0);
}

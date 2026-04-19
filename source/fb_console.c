#include "fb_console.h"

#include "defs.h"
#include "utility.h"
#include "virtio_gpu.h"
#include "memory.h"

#define GLYPH_W 8u
#define GLYPH_H 16u

typedef struct
{
    char c;
    uint8_t color;
} Cell;

static FramebufferInfo _framebuffer;
static bool _framebuffer_ok = false;

static Cell* _cells;
static size_t _columns;
static size_t _rows;

static bool _dirty;
static uint32_t _dirty_x0, _dirty_y0, _dirty_x1, _dirty_y1;

static inline Cell* cell_at(size_t x, size_t y)
{
    return &_cells[y * _columns + x];
}

static const uint32_t _vga16_xrgb[16] =
{
    0x00000000u, // black
    0x000000AAu, // blue
    0x0000AA00u, // green
    0x0000AAAAu, // cyan
    0x00AA0000u, // red
    0x00AA00AAu, // magenta
    0x00AA5500u, // brown
    0x00AAAAAAu, // light grey
    0x00555555u, // dark grey
    0x005555FFu, // light blue
    0x0055FF55u, // light green
    0x0055FFFFu, // light cyan
    0x00FF5555u, // light red
    0x00FF55FFu, // light magenta
    0x00FFFF55u, // light brown
    0x00FFFFFFu, // white
};

static inline uint32_t fg_from_color(uint8_t color)
{
    return _vga16_xrgb[color & 0x0Fu];
}

static inline uint32_t bg_from_color(uint8_t color)
{
    return _vga16_xrgb[(color >> 4) & 0x0Fu];
}

static inline void mark_dirty_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!_framebuffer_ok) 
    {
        return;
    }

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

static inline void put_pixel(uint32_t x, uint32_t y, uint32_t xrgb)
{
    if (!_framebuffer_ok || x >= _framebuffer.width || y >= _framebuffer.height) 
    {
        return;
    }

    uint32_t stride_pixels = _framebuffer.stride_bytes / 4u;
    _framebuffer.buffer[y * stride_pixels + x] = xrgb;
}

static void fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t xrgb)
{
    if (!_framebuffer_ok || x >= _framebuffer.width || y >= _framebuffer.height) 
    {
        return;
    }

    if (x + w > _framebuffer.width) 
    {
        w = _framebuffer.width - x;
    }
    if (y + h > _framebuffer.height)
    {
        h = _framebuffer.height - y;
    }

    uint32_t stride_pixels = _framebuffer.stride_bytes / 4u;

    for (uint32_t yy = 0; yy < h; yy++)
    {
        uint32_t* row = &_framebuffer.buffer[(y + yy) * stride_pixels + x];

        for (uint32_t xx = 0; xx < w; xx++)
        {
            row[xx] = xrgb;
        }
    }

    mark_dirty_rect(x, y, w, h);
}

typedef struct 
{ 
    char ch; 
    uint8_t rows[7];
} Glyph5x7;

static const Glyph5x7 _glyphs_5x7[] =
{
    // digits */
    { '0', { 0x1E,0x21,0x23,0x25,0x29,0x31,0x1E } },
    { '1', { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E } },
    { '2', { 0x1E,0x21,0x01,0x06,0x18,0x20,0x3F } },
    { '3', { 0x1E,0x21,0x01,0x0E,0x01,0x21,0x1E } },
    { '4', { 0x02,0x06,0x0A,0x12,0x3F,0x02,0x02 } },
    { '5', { 0x3F,0x20,0x3E,0x01,0x01,0x21,0x1E } },
    { '6', { 0x0E,0x10,0x20,0x3E,0x21,0x21,0x1E } },
    { '7', { 0x3F,0x01,0x02,0x04,0x08,0x10,0x10 } },
    { '8', { 0x1E,0x21,0x21,0x1E,0x21,0x21,0x1E } },
    { '9', { 0x1E,0x21,0x21,0x1F,0x01,0x02,0x1C } },

    // uppercase letters
    { 'A', { 0x0E,0x11,0x21,0x21,0x3F,0x21,0x21 } },
    { 'B', { 0x3E,0x21,0x21,0x3E,0x21,0x21,0x3E } },
    { 'C', { 0x1E,0x21,0x20,0x20,0x20,0x21,0x1E } },
    { 'D', { 0x3C,0x22,0x21,0x21,0x21,0x22,0x3C } },
    { 'E', { 0x3F,0x20,0x20,0x3E,0x20,0x20,0x3F } },
    { 'F', { 0x3F,0x20,0x20,0x3E,0x20,0x20,0x20 } },
    { 'G', { 0x1E,0x21,0x20,0x27,0x21,0x21,0x1E } },
    { 'H', { 0x21,0x21,0x21,0x3F,0x21,0x21,0x21 } },
    { 'I', { 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E } },
    { 'J', { 0x07,0x02,0x02,0x02,0x22,0x22,0x1C } },
    { 'K', { 0x21,0x22,0x24,0x38,0x24,0x22,0x21 } },
    { 'L', { 0x20,0x20,0x20,0x20,0x20,0x20,0x3F } },
    { 'M', { 0x21,0x33,0x2D,0x21,0x21,0x21,0x21 } },
    { 'N', { 0x21,0x31,0x29,0x25,0x23,0x21,0x21 } },
    { 'O', { 0x1E,0x21,0x21,0x21,0x21,0x21,0x1E } },
    { 'P', { 0x3E,0x21,0x21,0x3E,0x20,0x20,0x20 } },
    { 'Q', { 0x1E,0x21,0x21,0x21,0x25,0x22,0x1D } },
    { 'R', { 0x3E,0x21,0x21,0x3E,0x24,0x22,0x21 } },
    { 'S', { 0x1F,0x20,0x20,0x1E,0x01,0x01,0x3E } },
    { 'T', { 0x3F,0x04,0x04,0x04,0x04,0x04,0x04 } },
    { 'U', { 0x21,0x21,0x21,0x21,0x21,0x21,0x1E } },
    { 'V', { 0x21,0x21,0x21,0x21,0x21,0x12,0x0C } },
    { 'W', { 0x21,0x21,0x21,0x21,0x2D,0x33,0x21 } },
    { 'X', { 0x21,0x12,0x0C,0x0C,0x0C,0x12,0x21 } },
    { 'Y', { 0x21,0x12,0x0C,0x04,0x04,0x04,0x04 } },
    { 'Z', { 0x3F,0x01,0x02,0x04,0x08,0x10,0x3F } },

    // lowercase letters (6x7, shifted-left variants of common 5x7 shapes)
    { 'a', { 0x00,0x00,0x1C,0x02,0x1E,0x22,0x1E } },
    { 'b', { 0x20,0x20,0x3C,0x22,0x22,0x22,0x3C } },
    { 'c', { 0x00,0x00,0x1C,0x20,0x20,0x20,0x1C } },
    { 'd', { 0x02,0x02,0x1E,0x22,0x22,0x22,0x1E } },
    { 'e', { 0x00,0x00,0x1C,0x22,0x3E,0x20,0x1C } },
    { 'f', { 0x0C,0x10,0x3C,0x10,0x10,0x10,0x10 } },
    { 'g', { 0x00,0x00,0x1E,0x22,0x1E,0x02,0x1C } },
    { 'h', { 0x20,0x20,0x3C,0x22,0x22,0x22,0x22 } },
    { 'i', { 0x08,0x00,0x18,0x08,0x08,0x08,0x1C } },
    { 'j', { 0x04,0x00,0x0C,0x04,0x04,0x24,0x18 } },
    { 'k', { 0x20,0x24,0x28,0x30,0x28,0x24,0x22 } },
    { 'l', { 0x18,0x08,0x08,0x08,0x08,0x08,0x1C } },
    { 'm', { 0x00,0x00,0x34,0x2A,0x2A,0x2A,0x2A } },
    { 'n', { 0x00,0x00,0x3C,0x22,0x22,0x22,0x22 } },
    { 'o', { 0x00,0x00,0x1C,0x22,0x22,0x22,0x1C } },
    { 'p', { 0x00,0x00,0x3C,0x22,0x3C,0x20,0x20 } },
    { 'q', { 0x00,0x00,0x1E,0x22,0x1E,0x02,0x02 } },
    { 'r', { 0x00,0x00,0x2C,0x30,0x20,0x20,0x20 } },
    { 's', { 0x00,0x00,0x1E,0x20,0x1C,0x02,0x3C } },
    { 't', { 0x10,0x3C,0x10,0x10,0x10,0x10,0x0C } },
    { 'u', { 0x00,0x00,0x22,0x22,0x22,0x26,0x1A } },
    { 'v', { 0x00,0x00,0x22,0x22,0x14,0x14,0x08 } },
    { 'w', { 0x00,0x00,0x22,0x2A,0x2A,0x2A,0x14 } },
    { 'x', { 0x00,0x00,0x22,0x14,0x08,0x14,0x22 } },
    { 'y', { 0x00,0x00,0x22,0x22,0x1E,0x02,0x1C } },
    { 'z', { 0x00,0x00,0x3E,0x04,0x08,0x10,0x3E } },

    // symbols
    { '-', { 0x00,0x00,0x00,0x1F,0x00,0x00,0x00 } },
    { '.', { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C } },
    { '!', { 0x04,0x04,0x04,0x04,0x04,0x00,0x04 } },
    { ':', { 0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00 } },
    { ';', { 0x00,0x18,0x18,0x00,0x18,0x18,0x10 } },
    { '(', { 0x02,0x04,0x08,0x08,0x08,0x04,0x02 } },
    { ')', { 0x08,0x04,0x02,0x02,0x02,0x04,0x08 } },
    { '/', { 0x01,0x02,0x04,0x08,0x10,0x20,0x00 } },
    { '\\',{ 0x20,0x10,0x08,0x04,0x02,0x00,0x00 } },
    { ',', { 0x00,0x00,0x00,0x00,0x0C,0x0C,0x08 } },
    { '\'',{ 0x04,0x04,0x02,0x00,0x00,0x00,0x00 } },
    { '"',{ 0x0A,0x0A,0x04,0x00,0x00,0x00,0x00 } },
    { '?', { 0x1E,0x21,0x01,0x06,0x04,0x00,0x04 } },
    { '<', { 0x04,0x08,0x10,0x20,0x10,0x08,0x04 } },
    { '>', { 0x10,0x08,0x04,0x02,0x04,0x08,0x10 } },
    { '[', { 0x3C,0x20,0x20,0x20,0x20,0x20,0x3C } },
    { ']', { 0x3C,0x04,0x04,0x04,0x04,0x04,0x3C } },
    { '{', { 0x1C,0x10,0x10,0x20,0x10,0x10,0x1C } },
    { '}', { 0x38,0x08,0x08,0x04,0x08,0x08,0x38 } },
    { '+', { 0x00,0x08,0x08,0x3E,0x08,0x08,0x00 } },
    { '=', { 0x00,0x00,0x3E,0x00,0x3E,0x00,0x00 } },
    { '_', { 0x00,0x00,0x00,0x00,0x00,0x00,0x3E } },
    { '@', { 0x1C,0x22,0x2E,0x2A,0x2E,0x20,0x1C } },
    { '#', { 0x14,0x3E,0x14,0x14,0x3E,0x14,0x00 } },
    { '$', { 0x08,0x1E,0x28,0x1C,0x0A,0x3C,0x08 } },
    { '%', { 0x32,0x32,0x04,0x08,0x10,0x26,0x26 } },
    { '&', { 0x18,0x24,0x28,0x10,0x2A,0x24,0x1A } },
    { '*', { 0x00,0x14,0x08,0x3E,0x08,0x14,0x00 } },
    { '|', { 0x08,0x08,0x08,0x08,0x08,0x08,0x08 } },
    { ' ', { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
};

static bool get_5x7(char c, uint8_t out_rows[7])
{
    const size_t glyph_count = sizeof(_glyphs_5x7) / sizeof(_glyphs_5x7[0]);

    // Prefer exact-case lookups so lowercase can render distinctly.
    for (size_t i = 0; i < glyph_count; i++)
    {
        if (_glyphs_5x7[i].ch == c)
        {
            for (size_t j = 0; j < 7; j++)
            {
                out_rows[j] = _glyphs_5x7[i].rows[j];
            }

            return true;
        }
    }

    // Backward-compatible fallback: if lowercase not present, use uppercase.
    if (c >= 'a' && c <= 'z')
    {
        char upper = (char)(c - 'a' + 'A');
        for (size_t i = 0; i < glyph_count; i++)
        {
            if (_glyphs_5x7[i].ch == upper)
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

static void draw_box_char(uint8_t ch, uint32_t pixel_x, uint32_t pixel_y, uint32_t foreground, uint32_t background)
{
    fill_rect(pixel_x, pixel_y, GLYPH_W, GLYPH_H, background);

    const uint32_t x_midpoint = pixel_x + (GLYPH_W / 2);
    const uint32_t y_midpoint = pixel_y + (GLYPH_H / 2);

    const uint32_t x0 = pixel_x;
    const uint32_t x1 = pixel_x + GLYPH_W - 1;
    const uint32_t y0 = pixel_y;
    const uint32_t y1 = pixel_y + GLYPH_H - 1;

    switch (ch)
    {
        case 0xC4: // horizontal
            for (uint32_t x = x0; x <= x1; x++)
            {
                put_pixel(x, y_midpoint, foreground);
            }
            break;
        case 0xB3: // vertical
            for (uint32_t y = y0; y <= y1; y++)
            {
                put_pixel(x_midpoint, y, foreground);
            }
            break;
        case 0xDA: // top-left
            for (uint32_t x = x_midpoint; x <= x1; x++)
            {
                put_pixel(x, y_midpoint, foreground);
            }
            for (uint32_t y = y_midpoint; y <= y1; y++)
            {
                put_pixel(x_midpoint, y, foreground);
            }
            break;
        case 0xBF: // top-right
            for (uint32_t x = x0; x <= x_midpoint; x++)
            {
                put_pixel(x, y_midpoint, foreground);
            }
            for (uint32_t y = y_midpoint; y <= y1; y++)
            {
                put_pixel(x_midpoint, y, foreground);
            }
            break;
        case 0xC0: // bottom-left
            for (uint32_t x = x_midpoint; x <= x1; x++)
            {
                put_pixel(x, y_midpoint, foreground);
            }
            for (uint32_t y = y0; y <= y_midpoint; y++)
            {
                put_pixel(x_midpoint, y, foreground);
            }
            break;
        case 0xD9: // bottom-right
            for (uint32_t x = x0; x <= x_midpoint; x++)
            {
                put_pixel(x, y_midpoint, foreground);
            }
            for (uint32_t y = y0; y <= y_midpoint; y++)
            {
                put_pixel(x_midpoint, y, foreground);
            }
            break;
        default:
            break;
    }

    mark_dirty_rect(pixel_x, pixel_y, GLYPH_W, GLYPH_H);
}

static void draw_glyph(char c, uint8_t color, uint32_t cell_x, uint32_t cell_y)
{
    const uint32_t foreground = fg_from_color(color);
    const uint32_t background = bg_from_color(color);

    const uint32_t pixel_x = cell_x * GLYPH_W;
    const uint32_t pixel_y = cell_y * GLYPH_H;

    if (!_framebuffer_ok)
    {
        return;
    }

    if ((unsigned char)c >= 0x80)
    {
        draw_box_char((uint8_t)c, pixel_x, pixel_y, foreground, background);
        return;
    }

    fill_rect(pixel_x, pixel_y, GLYPH_W, GLYPH_H, background);

    uint8_t rows[7];
    if (!get_5x7(c, rows))
    {
        (void)get_5x7('?', rows);
    }

    // glyph table uses 6-bit rows rendering as 6x7
    // center 6x7 inside 8x16: x offset 1, y offset 4
    const uint32_t x0 = pixel_x + 1;
    const uint32_t y0 = pixel_y + 4;

    for (uint32_t r = 0; r < 7; r++)
    {
        uint8_t bits = rows[r];
        for (uint32_t col = 0; col < 6; col++)
        {
            if (bits & (1u << (5 - col)))
            {
                put_pixel(x0 + col, y0 + r, foreground);
            }
        }
    }

    mark_dirty_rect(pixel_x, pixel_y, GLYPH_W, GLYPH_H);
}

void terminal_initialize(void)
{
    memory_init();

    if (!virtio_gpu_init(&_framebuffer))
    {
        _framebuffer_ok = false;
        return;
    }

    _framebuffer_ok = true;
    _dirty = false;

    _active_color = (uint8_t)((VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLUE);

    _columns = _framebuffer.width / GLYPH_W;
    _rows = _framebuffer.height / GLYPH_H;

    if (_columns < 40) _columns = 40;
    if (_rows < 15) _rows = 15;

    _cells = (Cell*)kmalloc_aligned(sizeof(Cell) * _columns * _rows, 16);
    if (!_cells)
    {
        printf("fb_console: cell alloc failed\n");
        _framebuffer_ok = false;
        return;
    }

    fill_rect(0, 0, _framebuffer.width, _framebuffer.height, bg_from_color(_active_color));

    for (size_t y = 0; y < _rows; y++)
    {
        for (size_t x = 0; x < _columns; x++)
        {
            Cell* cell = cell_at(x, y);
            cell->c = ' ';
            cell->color = _active_color;
        }
    }

    terminal_flush();
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    if (!_framebuffer_ok || x >= _columns || y >= _rows) 
    {
        return;
    }

    Cell* cell = cell_at(x, y);
    cell->c = c;
    cell->color = color;

    draw_glyph(c, color, (uint32_t)x, (uint32_t)y);
}

void terminal_putchar(char c, size_t* x, size_t* y)
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

    terminal_putentryat(c, _active_color, *x, *y);

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

void terminal_write(const char* data, size_t size, size_t x, size_t y)
{
    for (size_t i = 0; i < size; i++)
    {
        terminal_putchar(data[i], &x, &y);
    }
}

void terminal_writestring(const char* data, size_t x, size_t y)
{
    terminal_write(data, strlen(data), x, y);
}

bool terminal_getentryat(size_t x, size_t y, char* out_c, uint8_t* out_color)
{
    if (!_framebuffer_ok || x >= _columns || y >= _rows)
    {
        return false;
    }

    Cell* cell = cell_at(x, y);
    if (out_c)
    {
        *out_c = cell->c;
    }
    if (out_color)
    {
        *out_color = cell->color;
    }
    return true;
}

void terminal_get_size(size_t* out_cols, size_t* out_rows)
{
    if (out_cols) 
    {
        *out_cols = _columns;
    }
    if (out_rows)
    {
        *out_rows = _rows;
    }
}

void terminal_flush(void)
{
    if (!_framebuffer_ok || !_dirty)
    {
        return;
    }

    uint32_t x0 = _dirty_x0;
    uint32_t y0 = _dirty_y0;
    uint32_t x1 = _dirty_x1;
    uint32_t y1 = _dirty_y1;

    _dirty = false;

    if (x1 <= x0 || y1 <= y0) 
    {
        return;
    }

    virtio_gpu_flush_rect(x0, y0, x1 - x0, y1 - y0);
}

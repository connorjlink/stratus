// Stratus: framebuffer.zig
// (c) 2026 Connor J. Link. All Rights Reserved.

const GLYPH_W: u32 = 8;
const GLYPH_H: u32 = 16;

const VGA_COLOR_BLUE: u8 = 1;
const VGA_COLOR_LIGHT_GREY: u8 = 7;

const FramebufferInfo = extern struct {
    width: u32,
    height: u32,
    stride_bytes: u32,
    buffer: [*]u32,
};

const Cell = packed struct {
    c: c_char,
    color: u8,
};

const Glyph5x7 = struct {
    ch: u8,
    rows: [7]u8,
};

extern fn memory_init() void;
// TODO: replace virtio gpu with DMA GPU initializaion by MMIO calls
extern fn virtio_gpu_init(out_fb: *FramebufferInfo) bool;
extern fn virtio_gpu_flush_rect(x: u32, y: u32, w: u32, h: u32) void;
extern fn kmalloc_aligned(size: usize, alignment: usize) ?*anyopaque;

pub export var _active_color: u8 = 0;

var framebuffer: FramebufferInfo = undefined;
var framebuffer_ok: bool = false;

var cells: ?[*]Cell = null;
var columns: usize = 0;
var rows: usize = 0;

var dirty: bool = false;
var dirty_x0: u32 = 0;
var dirty_y0: u32 = 0;
var dirty_x1: u32 = 0;
var dirty_y1: u32 = 0;

fn cCharToU8(c: c_char) u8 {
    return @as(u8, @bitCast(c));
}

fn u8ToCChar(v: u8) c_char {
    return @as(c_char, @bitCast(v));
}

fn cellAt(x: usize, y: usize) *Cell {
    return &cells.?[y * columns + x];
}

const vga16_xrgb = [16]u32{
    0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
    0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
    0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
    0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
};

fn fgFromColor(color: u8) u32 {
    return vga16_xrgb[color & 0x0F];
}

fn bgFromColor(color: u8) u32 {
    return vga16_xrgb[(color >> 4) & 0x0F];
}

fn markDirtyRect(x: u32, y: u32, w: u32, h: u32) void {
    if (!framebuffer_ok) return;

    if (!dirty) {
        dirty = true;
        dirty_x0 = x;
        dirty_y0 = y;
        dirty_x1 = x + w;
        dirty_y1 = y + h;
        return;
    }

    if (x < dirty_x0) dirty_x0 = x;
    if (y < dirty_y0) dirty_y0 = y;
    if (x + w > dirty_x1) dirty_x1 = x + w;
    if (y + h > dirty_y1) dirty_y1 = y + h;
}

fn putPixel(x: u32, y: u32, xrgb: u32) void {
    if (!framebuffer_ok or x >= framebuffer.width or y >= framebuffer.height) return;
    const stride_pixels: u32 = framebuffer.stride_bytes / 4;
    framebuffer.buffer[y * stride_pixels + x] = xrgb;
}

fn fillRect(x_in: u32, y_in: u32, w_in: u32, h_in: u32, xrgb: u32) void {
    if (!framebuffer_ok or x_in >= framebuffer.width or y_in >= framebuffer.height) return;

    var x = x_in;
    var y = y_in;
    var w = w_in;
    var h = h_in;

    if (x + w > framebuffer.width) w = framebuffer.width - x;
    if (y + h > framebuffer.height) h = framebuffer.height - y;

    const stride_pixels: u32 = framebuffer.stride_bytes / 4;

    var yy: u32 = 0;
    while (yy < h) : (yy += 1) {
        const row = framebuffer.buffer[(y + yy) * stride_pixels + x ..];
        var xx: u32 = 0;
        while (xx < w) : (xx += 1) {
            row[xx] = xrgb;
        }
    }

    markDirtyRect(x, y, w, h);
}

const glyphs_5x7 = [_]Glyph5x7{
    .{ .ch = '0', .rows = .{ 0x1E,0x21,0x23,0x25,0x29,0x31,0x1E } },
    .{ .ch = '1', .rows = .{ 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E } },
    .{ .ch = '2', .rows = .{ 0x1E,0x21,0x01,0x06,0x18,0x20,0x3F } },
    .{ .ch = '3', .rows = .{ 0x1E,0x21,0x01,0x0E,0x01,0x21,0x1E } },
    .{ .ch = '4', .rows = .{ 0x02,0x06,0x0A,0x12,0x3F,0x02,0x02 } },
    .{ .ch = '5', .rows = .{ 0x3F,0x20,0x3E,0x01,0x01,0x21,0x1E } },
    .{ .ch = '6', .rows = .{ 0x0E,0x10,0x20,0x3E,0x21,0x21,0x1E } },
    .{ .ch = '7', .rows = .{ 0x3F,0x01,0x02,0x04,0x08,0x10,0x10 } },
    .{ .ch = '8', .rows = .{ 0x1E,0x21,0x21,0x1E,0x21,0x21,0x1E } },
    .{ .ch = '9', .rows = .{ 0x1E,0x21,0x21,0x1F,0x01,0x02,0x1C } },

    .{ .ch = 'A', .rows = .{ 0x0E,0x11,0x21,0x21,0x3F,0x21,0x21 } },
    .{ .ch = 'B', .rows = .{ 0x3E,0x21,0x21,0x3E,0x21,0x21,0x3E } },
    .{ .ch = 'C', .rows = .{ 0x1E,0x21,0x20,0x20,0x20,0x21,0x1E } },
    .{ .ch = 'D', .rows = .{ 0x3C,0x22,0x21,0x21,0x21,0x22,0x3C } },
    .{ .ch = 'E', .rows = .{ 0x3F,0x20,0x20,0x3E,0x20,0x20,0x3F } },
    .{ .ch = 'F', .rows = .{ 0x3F,0x20,0x20,0x3E,0x20,0x20,0x20 } },
    .{ .ch = 'G', .rows = .{ 0x1E,0x21,0x20,0x27,0x21,0x21,0x1E } },
    .{ .ch = 'H', .rows = .{ 0x21,0x21,0x21,0x3F,0x21,0x21,0x21 } },
    .{ .ch = 'I', .rows = .{ 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E } },
    .{ .ch = 'J', .rows = .{ 0x07,0x02,0x02,0x02,0x22,0x22,0x1C } },
    .{ .ch = 'K', .rows = .{ 0x21,0x22,0x24,0x38,0x24,0x22,0x21 } },
    .{ .ch = 'L', .rows = .{ 0x20,0x20,0x20,0x20,0x20,0x20,0x3F } },
    .{ .ch = 'M', .rows = .{ 0x21,0x33,0x2D,0x21,0x21,0x21,0x21 } },
    .{ .ch = 'N', .rows = .{ 0x21,0x31,0x29,0x25,0x23,0x21,0x21 } },
    .{ .ch = 'O', .rows = .{ 0x1E,0x21,0x21,0x21,0x21,0x21,0x1E } },
    .{ .ch = 'P', .rows = .{ 0x3E,0x21,0x21,0x3E,0x20,0x20,0x20 } },
    .{ .ch = 'Q', .rows = .{ 0x1E,0x21,0x21,0x21,0x25,0x22,0x1D } },
    .{ .ch = 'R', .rows = .{ 0x3E,0x21,0x21,0x3E,0x24,0x22,0x21 } },
    .{ .ch = 'S', .rows = .{ 0x1F,0x20,0x20,0x1E,0x01,0x01,0x3E } },
    .{ .ch = 'T', .rows = .{ 0x3F,0x04,0x04,0x04,0x04,0x04,0x04 } },
    .{ .ch = 'U', .rows = .{ 0x21,0x21,0x21,0x21,0x21,0x21,0x1E } },
    .{ .ch = 'V', .rows = .{ 0x21,0x21,0x21,0x21,0x21,0x12,0x0C } },
    .{ .ch = 'W', .rows = .{ 0x21,0x21,0x21,0x21,0x2D,0x33,0x21 } },
    .{ .ch = 'X', .rows = .{ 0x21,0x12,0x0C,0x0C,0x0C,0x12,0x21 } },
    .{ .ch = 'Y', .rows = .{ 0x21,0x12,0x0C,0x04,0x04,0x04,0x04 } },
    .{ .ch = 'Z', .rows = .{ 0x3F,0x01,0x02,0x04,0x08,0x10,0x3F } },

    .{ .ch = 'a', .rows = .{ 0x00,0x00,0x1C,0x02,0x1E,0x22,0x1E } },
    .{ .ch = 'b', .rows = .{ 0x20,0x20,0x3C,0x22,0x22,0x22,0x3C } },
    .{ .ch = 'c', .rows = .{ 0x00,0x00,0x1C,0x20,0x20,0x20,0x1C } },
    .{ .ch = 'd', .rows = .{ 0x02,0x02,0x1E,0x22,0x22,0x22,0x1E } },
    .{ .ch = 'e', .rows = .{ 0x00,0x00,0x1C,0x22,0x3E,0x20,0x1C } },
    .{ .ch = 'f', .rows = .{ 0x0C,0x10,0x3C,0x10,0x10,0x10,0x10 } },
    .{ .ch = 'g', .rows = .{ 0x00,0x00,0x1E,0x22,0x1E,0x02,0x1C } },
    .{ .ch = 'h', .rows = .{ 0x20,0x20,0x3C,0x22,0x22,0x22,0x22 } },
    .{ .ch = 'i', .rows = .{ 0x08,0x00,0x18,0x08,0x08,0x08,0x1C } },
    .{ .ch = 'j', .rows = .{ 0x04,0x00,0x0C,0x04,0x04,0x24,0x18 } },
    .{ .ch = 'k', .rows = .{ 0x20,0x24,0x28,0x30,0x28,0x24,0x22 } },
    .{ .ch = 'l', .rows = .{ 0x18,0x08,0x08,0x08,0x08,0x08,0x1C } },
    .{ .ch = 'm', .rows = .{ 0x00,0x00,0x34,0x2A,0x2A,0x2A,0x2A } },
    .{ .ch = 'n', .rows = .{ 0x00,0x00,0x3C,0x22,0x22,0x22,0x22 } },
    .{ .ch = 'o', .rows = .{ 0x00,0x00,0x1C,0x22,0x22,0x22,0x1C } },
    .{ .ch = 'p', .rows = .{ 0x00,0x00,0x3C,0x22,0x3C,0x20,0x20 } },
    .{ .ch = 'q', .rows = .{ 0x00,0x00,0x1E,0x22,0x1E,0x02,0x02 } },
    .{ .ch = 'r', .rows = .{ 0x00,0x00,0x2C,0x30,0x20,0x20,0x20 } },
    .{ .ch = 's', .rows = .{ 0x00,0x00,0x1E,0x20,0x1C,0x02,0x3C } },
    .{ .ch = 't', .rows = .{ 0x10,0x3C,0x10,0x10,0x10,0x10,0x0C } },
    .{ .ch = 'u', .rows = .{ 0x00,0x00,0x22,0x22,0x22,0x26,0x1A } },
    .{ .ch = 'v', .rows = .{ 0x00,0x00,0x22,0x22,0x14,0x14,0x08 } },
    .{ .ch = 'w', .rows = .{ 0x00,0x00,0x22,0x2A,0x2A,0x2A,0x14 } },
    .{ .ch = 'x', .rows = .{ 0x00,0x00,0x22,0x14,0x08,0x14,0x22 } },
    .{ .ch = 'y', .rows = .{ 0x00,0x00,0x22,0x22,0x1E,0x02,0x1C } },
    .{ .ch = 'z', .rows = .{ 0x00,0x00,0x3E,0x04,0x08,0x10,0x3E } },

    .{ .ch = '-', .rows = .{ 0x00,0x00,0x00,0x1F,0x00,0x00,0x00 } },
    .{ .ch = '.', .rows = .{ 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C } },
    .{ .ch = '!', .rows = .{ 0x04,0x04,0x04,0x04,0x04,0x00,0x04 } },
    .{ .ch = ':', .rows = .{ 0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00 } },
    .{ .ch = ';', .rows = .{ 0x00,0x18,0x18,0x00,0x18,0x18,0x10 } },
    .{ .ch = '(', .rows = .{ 0x02,0x04,0x08,0x08,0x08,0x04,0x02 } },
    .{ .ch = ')', .rows = .{ 0x08,0x04,0x02,0x02,0x02,0x04,0x08 } },
    .{ .ch = '/', .rows = .{ 0x01,0x02,0x04,0x08,0x10,0x20,0x00 } },
    .{ .ch = '\\', .rows = .{ 0x20,0x10,0x08,0x04,0x02,0x00,0x00 } },
    .{ .ch = ',', .rows = .{ 0x00,0x00,0x00,0x00,0x0C,0x0C,0x08 } },
    .{ .ch = '\'', .rows = .{ 0x04,0x04,0x02,0x00,0x00,0x00,0x00 } },
    .{ .ch = '"', .rows = .{ 0x0A,0x0A,0x04,0x00,0x00,0x00,0x00 } },
    .{ .ch = '?', .rows = .{ 0x1E,0x21,0x01,0x06,0x04,0x00,0x04 } },
    .{ .ch = '<', .rows = .{ 0x04,0x08,0x10,0x20,0x10,0x08,0x04 } },
    .{ .ch = '>', .rows = .{ 0x10,0x08,0x04,0x02,0x04,0x08,0x10 } },
    .{ .ch = '[', .rows = .{ 0x3C,0x20,0x20,0x20,0x20,0x20,0x3C } },
    .{ .ch = ']', .rows = .{ 0x3C,0x04,0x04,0x04,0x04,0x04,0x3C } },
    .{ .ch = '{', .rows = .{ 0x1C,0x10,0x10,0x20,0x10,0x10,0x1C } },
    .{ .ch = '}', .rows = .{ 0x38,0x08,0x08,0x04,0x08,0x08,0x38 } },
    .{ .ch = '+', .rows = .{ 0x00,0x08,0x08,0x3E,0x08,0x08,0x00 } },
    .{ .ch = '=', .rows = .{ 0x00,0x00,0x3E,0x00,0x3E,0x00,0x00 } },
    .{ .ch = '_', .rows = .{ 0x00,0x00,0x00,0x00,0x00,0x00,0x3E } },
    .{ .ch = '@', .rows = .{ 0x1C,0x22,0x2E,0x2A,0x2E,0x20,0x1C } },
    .{ .ch = '#', .rows = .{ 0x14,0x3E,0x14,0x14,0x3E,0x14,0x00 } },
    .{ .ch = '$', .rows = .{ 0x08,0x1E,0x28,0x1C,0x0A,0x3C,0x08 } },
    .{ .ch = '%', .rows = .{ 0x32,0x32,0x04,0x08,0x10,0x26,0x26 } },
    .{ .ch = '&', .rows = .{ 0x18,0x24,0x28,0x10,0x2A,0x24,0x1A } },
    .{ .ch = '*', .rows = .{ 0x00,0x14,0x08,0x3E,0x08,0x14,0x00 } },
    .{ .ch = '|', .rows = .{ 0x08,0x08,0x08,0x08,0x08,0x08,0x08 } },
    .{ .ch = ' ', .rows = .{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
};

fn get5x7(c: u8, out_rows: *[7]u8) bool {
    for (glyphs_5x7) |g| {
        if (g.ch == c) {
            out_rows.* = g.rows;
            return true;
        }
    }

    if (c >= 'a' and c <= 'z') {
        const upper = c - 'a' + 'A';
        for (glyphs_5x7) |g| {
            if (g.ch == upper) {
                out_rows.* = g.rows;
                return true;
            }
        }
    }

    return false;
}

fn drawBoxChar(ch: u8, pixel_x: u32, pixel_y: u32, foreground: u32, background: u32) void {
    fillRect(pixel_x, pixel_y, GLYPH_W, GLYPH_H, background);

    const x_mid = pixel_x + (GLYPH_W / 2);
    const y_mid = pixel_y + (GLYPH_H / 2);

    const x0 = pixel_x;
    const x1 = pixel_x + GLYPH_W - 1;
    const y0 = pixel_y;
    const y1 = pixel_y + GLYPH_H - 1;

    switch (ch) {
        0xC4 => { var x = x0; while (x <= x1) : (x += 1) putPixel(x, y_mid, foreground); },
        0xB3 => { var y = y0; while (y <= y1) : (y += 1) putPixel(x_mid, y, foreground); },
        0xDA => {
            var x = x_mid; while (x <= x1) : (x += 1) putPixel(x, y_mid, foreground);
            var y = y_mid; while (y <= y1) : (y += 1) putPixel(x_mid, y, foreground);
        },
        0xBF => {
            var x = x0; while (x <= x_mid) : (x += 1) putPixel(x, y_mid, foreground);
            var y = y_mid; while (y <= y1) : (y += 1) putPixel(x_mid, y, foreground);
        },
        0xC0 => {
            var x = x_mid; while (x <= x1) : (x += 1) putPixel(x, y_mid, foreground);
            var y = y0; while (y <= y_mid) : (y += 1) putPixel(x_mid, y, foreground);
        },
        0xD9 => {
            var x = x0; while (x <= x_mid) : (x += 1) putPixel(x, y_mid, foreground);
            var y = y0; while (y <= y_mid) : (y += 1) putPixel(x_mid, y, foreground);
        },
        else => {},
    }

    markDirtyRect(pixel_x, pixel_y, GLYPH_W, GLYPH_H);
}

fn drawGlyph(c: u8, color: u8, cell_x: u32, cell_y: u32) void {
    const fg = fgFromColor(color);
    const bg = bgFromColor(color);

    const pixel_x = cell_x * GLYPH_W;
    const pixel_y = cell_y * GLYPH_H;

    if (!framebuffer_ok) return;

    if (c >= 0x80) {
        drawBoxChar(c, pixel_x, pixel_y, fg, bg);
        return;
    }

    fillRect(pixel_x, pixel_y, GLYPH_W, GLYPH_H, bg);

    var rows7: [7]u8 = undefined;
    if (!get5x7(c, &rows7)) _ = get5x7('?', &rows7);

    const x0 = pixel_x + 1;
    const y0 = pixel_y + 4;

    var r: u32 = 0;
    while (r < 7) : (r += 1) {
        const bits = rows7[r];
        var col: u32 = 0;
        while (col < 6) : (col += 1) {
            if ((bits & (@as(u8, 1) << @intCast(5 - col))) != 0) {
                putPixel(x0 + col, y0 + r, fg);
            }
        }
    }

    markDirtyRect(pixel_x, pixel_y, GLYPH_W, GLYPH_H);
}

fn cstrLen(s: [*:0]const u8) usize {
    var i: usize = 0;
    while (s[i] != 0) : (i += 1) 
    {
    }
    return i;
}

pub export fn terminal_initialize() void {
    memory_init();

    if (!virtio_gpu_init(&framebuffer)) {
        framebuffer_ok = false;
        return;
    }

    framebuffer_ok = true;
    dirty = false;

    _active_color = (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLUE;

    columns = framebuffer.width / GLYPH_W;
    rows = framebuffer.height / GLYPH_H;

    if (columns < 40) columns = 40;
    if (rows < 15) rows = 15;

    const raw = kmalloc_aligned(@sizeOf(Cell) * columns * rows, 16) orelse {
        framebuffer_ok = false;
        return;
    };
    cells = @as([*]Cell, @ptrCast(raw));

    fillRect(0, 0, framebuffer.width, framebuffer.height, bgFromColor(_active_color));

    var y: usize = 0;
    while (y < rows) : (y += 1) {
        var x: usize = 0;
        while (x < columns) : (x += 1) {
            const c = cellAt(x, y);
            c.c = ' ';
            c.color = _active_color;
        }
    }

    terminal_flush();
}

pub export fn terminal_putentryat(c: c_char, color: u8, x: usize, y: usize) void {
    if (!framebuffer_ok or x >= columns or y >= rows) return;

    const cell = cellAt(x, y);
    cell.c = c;
    cell.color = color;

    drawGlyph(cCharToU8(c), color, @intCast(x), @intCast(y));
}

pub export fn terminal_putchar(c: c_char, x: ?*usize, y: ?*usize) void {
    if (x == null or y == null) return;
    const px = x.?;
    const py = y.?;

    switch (c) {
        '\n' => {
            px.* = 0;
            py.* += 1;
            return;
        },
        '\r' => {
            px.* = 0;
            return;
        },
        0 => return,
        else => {},
    }

    terminal_putentryat(c, _active_color, px.*, py.*);

    px.* += 1;
    if (px.* == columns) {
        px.* = 0;
        py.* += 1;
        if (py.* == rows) py.* = 0;
    }
}

pub export fn terminal_write(data: [*]const u8, size: usize, x: usize, y: usize) void {
    var cx = x;
    var cy = y;

    var i: usize = 0;
    while (i < size) : (i += 1) {
        terminal_putchar(u8ToCChar(data[i]), &cx, &cy);
    }
}

pub export fn terminal_writestring(data: [*:0]const u8, x: usize, y: usize) void {
    terminal_write(@as([*]const u8, @ptrCast(data)), cstrLen(data), x, y);
}

pub export fn terminal_getentryat(x: usize, y: usize, out_c: ?*c_char, out_color: ?*u8) bool {
    if (!framebuffer_ok or x >= columns or y >= rows) {
        return false;
    }

    const cell = cellAt(x, y);
    if (out_c) |p| p.* = cell.c;
    if (out_color) |p| p.* = cell.color;
    return true;
}

pub export fn terminal_get_size(out_cols: ?*usize, out_rows: ?*usize) void {
    if (out_cols) |p| p.* = columns;
    if (out_rows) |p| p.* = rows;
}

pub export fn terminal_flush() void {
    if (!framebuffer_ok or !dirty) {
        return;
    }

    const x0 = dirty_x0;
    const y0 = dirty_y0;
    const x1 = dirty_x1;
    const y1 = dirty_y1;

    dirty = false;

    if (x1 <= x0 or y1 <= y0) {
        return;
    }

    virtio_gpu_flush_rect(x0, y0, x1 - x0, y1 - y0);
}

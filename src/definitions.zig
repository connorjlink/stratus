// Stratus: definitions.zig
// (c) 2026 Connor J. Link. All Rights Reserved.

pub const VGAColor = enum(u8) {
    black = 0,
    blue = 1,
    green = 2,
    cyan = 3,
    red = 4,
    magenta = 5,
    brown = 6,
    light_grey = 7,
    dark_grey = 8,
    light_blue = 9,
    light_green = 10,
    light_cyan = 11,
    light_red = 12,
    light_magenta = 13,
    light_brown = 14,
    white = 15,
};

pub const VGAPalette = struct {
    foreground: VGAColor,
    background: VGAColor,
};

pub inline fn palette(foreground: VGAColor, background: VGAColor) VGAPalette {
    return .{ .foreground = foreground, .background = background };
}

pub inline fn invertPalette(p: VGAPalette) VGAPalette {
    return .{ .foreground = p.background, .background = p.foreground };
}

pub const VGACharacter = struct {
    c: u8,
    x: usize,
    y: usize,
    palette: VGAPalette,
};

pub inline fn character(c: u8, x: usize, y: usize, p: VGAPalette) VGACharacter {
    return .{ .c = c, .x = x, .y = y, .palette = p };
}

pub const Point = struct {
    x: usize,
    y: usize,
};

pub inline fn point(x: usize, y: usize) Point {
    return .{ .x = x, .y = y };
}

pub const Rect = struct {
    pos: Point,
    size: Point,
};

pub inline fn rect(pos: Point, size: Point) Rect {
    return .{ .pos = pos, .size = size };
}

pub const _explorer_rect: Rect = .{ .pos = .{ .x = 0, .y = 1 }, .size = .{ .x = 20, .y = 22 } };
pub const _console_rect: Rect = .{ .pos = .{ .x = 21, .y = 15 }, .size = .{ .x = 58, .y = 8 } };
pub const _navigator_rect: Rect = .{ .pos = .{ .x = 21, .y = 1 }, .size = .{ .x = 58, .y = 13 } };

pub const _console_cursor: Point = .{ .x = 0, .y = 0 };

pub const _active_color: u8 = 0;

pub fn layoutInit(cols: usize, rows: usize) void {
    if (cols < 40 or rows < 15) {
        return;
    }

    const content_h = rows - 2;

    var explorer_delta = cols / 4;
    if (explorer_delta < 20) {
        explorer_delta = 20;
    }
    if (explorer_delta > cols - 22) {
        explorer_delta = cols - 22;
    }

    const right_left = explorer_delta + 1;
    const right_delta = (cols - 1) - right_left;

    var console_h = content_h / 3;
    if (console_h < 9) {
        console_h = 9;
    }
    if (console_h > content_h - 6) {
        console_h = content_h - 6;
    }

    const navigator_h = content_h - console_h;

    _explorer_rect = .{
        .pos = .{ .x = 0, .y = 1 },
        .size = .{ .x = explorer_delta, .y = content_h - 1 },
    };
    _navigator_rect = .{
        .pos = .{ .x = right_left, .y = 1 },
        .size = .{ .x = right_delta, .y = navigator_h - 1 },
    };
    _console_rect = .{
        .pos = .{ .x = right_left, .y = 1 + navigator_h },
        .size = .{ .x = right_delta, .y = console_h - 1 },
    };
}

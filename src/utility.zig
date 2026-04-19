// Stratus: utility.zig
// (c) 2026 Connor J. Link. All Rights Reserved.

const std = @import("std");

pub fn max(x: usize, y: usize) usize {
    return if (x > y) x else y;
}

pub fn min(x: usize, y: usize) usize {
    return if (y > x) x else y;
}

pub fn strlen(string: [*:0]const u8) usize {
    var length: usize = 0;
    while (string[length] != 0) {
        length += 1;
    }
    return length;
}

pub fn strcpy(destination: [*]u8, string: [*:0]const u8) [*]u8 {
    var local_destination = destination;
    var local_source = string;
    while (local_source[0] != 0) {
        local_destination[0] = local_source[0];
        local_destination += 1;
        local_source += 1;
    }
    local_destination[0] = 0;
    return destination;
}

pub fn strcmp(string1: [*:0]const u8, string2: [*:0]const u8) i32 {
    var local_string1 = string1;
    var local_string2 = string2;
    while (local_string1[0] != 0 and local_string2[0] != 0) {
        if (local_string1[0] != local_string2[0]) {
            break;
        }
        local_string1 += 1;
        local_string2 += 1;
    }
    return @as(i32, local_string1[0]) - @as(i32, local_string2[0]);
}

pub fn memset(pointer: [*]u8, value: u8, number: usize) [*]u8 {
    var local_pointer = pointer;
    var local_number = number;
    while (local_number > 0) {
        local_pointer[0] = value;
        local_pointer += 1;
        local_number -= 1;
    }
    return pointer;
}

pub fn memcpy(destination: [*]u8, source: [*]const u8, number: usize) [*]u8 {
    var local_destination = destination;
    var local_source = source;
    var local_number = number;
    while (local_number > 0) {
        local_destination[0] = local_source[0];
        local_destination += 1;
        local_source += 1;
        local_number -= 1;
    }
    return destination;
}

inline fn platform_putchar(c: u8) void {
    const thr = @as(*volatile u8, @ptrFromInt(0x10000000));
    const lsr = @as(*volatile u8, @ptrFromInt(0x10000005));

    while ((lsr.* & (1 << 5)) == 0) {}
    thr.* = c;
}

pub fn putchar(c: u8) void {
    if (c == '\n') {
        platform_putchar('\r');
    }
    platform_putchar(c);
}

pub fn printf(format: [*:0]const u8, args: anytype) void {
    var fmt = format;
    var arg_index: usize = 0;

    while (fmt[0] != 0) {
        if (fmt[0] == '%') {
            fmt += 1;

            switch (fmt[0]) {
                'c' => {
                    const c = args[arg_index];
                    arg_index += 1;
                    putchar(@as(u8, @intCast(c)));
                },
                's' => {
                    const str = args[arg_index];
                    arg_index += 1;
                    var s = str;
                    while (s[0] != 0) {
                        putchar(s[0]);
                        s += 1;
                    }
                },
                'd' => {
                    var number = args[arg_index];
                    arg_index += 1;
                    var is_negative = false;

                    if (number < 0) {
                        is_negative = true;
                        number = -number;
                    }

                    var buffer: [12]u8 = undefined;
                    var i: usize = 0;

                    while (i < buffer.len) {
                        buffer[i] = @as(u8, @intCast((number % 10) + '0'));
                        i += 1;
                        number /= 10;
                        if (number == 0) break;
                    }

                    if (is_negative) {
                        buffer[i] = '-';
                        i += 1;
                    }

                    while (i > 0) {
                        i -= 1;
                        putchar(buffer[i]);
                    }
                },
                'u' => {
                    var number = args[arg_index];
                    arg_index += 1;
                    var buffer: [12]u8 = undefined;
                    var i: usize = 0;

                    while (i < buffer.len) {
                        buffer[i] = @as(u8, @intCast((number % 10) + '0'));
                        i += 1;
                        number /= 10;
                        if (number == 0) break;
                    }

                    while (i > 0) {
                        i -= 1;
                        putchar(buffer[i]);
                    }
                },
                'x' => {
                    const number = args[arg_index];
                    arg_index += 1;
                    const hex_digits = "0123456789ABCDEF";
                    var shift: u32 = (@sizeOf(@TypeOf(number)) * 8) - 4;

                    while (true) {
                        const nibble = @as(u4, @intCast((number >> shift) & 0xF));
                        putchar(hex_digits[nibble]);
                        if (shift == 0) break;
                        shift -= 4;
                    }
                },
                '%' => {
                    putchar('%');
                },
                0 => {
                    putchar('%');
                    return;
                },
                else => return,
            }
        } else {
            putchar(fmt[0]);
        }

        fmt += 1;
    }
}

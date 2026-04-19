// Stratus: mmio.zig
// (c) 2026 Connor J. Link. All Rights Reserved.

// kernel controls all of high memory
pub const KERNEL_BASE = 0x80000000;

// R4G4B4A4, 16bpp
pub const COLOR_DEPTH = 4;

pub const TEXTURE_BASE = 0xD0000000;
pub const TEXTURE_WIDTH = 256;
pub const TEXTURE_HEIGHT = 256;
pub const TEXTURE_SIZE = TEXTURE_WIDTH * TEXTURE_HEIGHT * COLOR_DEPTH;
pub const TEXTURE_SLOTS = 16;

pub fn textureAddress(slot: u32) u32 {
    return TEXTURE_BASE + (slot * TEXTURE_SIZE);
}

pub const FRAMEBUFFER_BASE = 0xE0000000;
pub const FRAMEBUFFER_WIDTH = 640;
pub const FRAMEBUFFER_HEIGHT = 480;
pub const FRAMEBUFFER_SIZE = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * COLOR_DEPTH;
pub const FRAMEBUFFER_SLOTS = 3 + 1;

pub fn framebufferAddress(slot: u32) u32 {
    return FRAMEBUFFER_BASE + (slot * FRAMEBUFFER_SIZE);
}

pub const MMIO_BASE = 0xF0000000;

// framebuffer commands 
pub const MMIO_REQUEST_FRAMEBUFFER = MMIO_BASE + 0x0000;
pub const MMIO_FRAMEBUFFER_BUSY = MMIO_BASE + 0x0004;
pub const MMIO_FRAMEBUFFER_RESPONSE = MMIO_BASE + 0x0008;

// framebuffer clipping rect
pub const MMIO_SET_RECT_X = MMIO_BASE + 0x0010;
pub const MMIO_SET_RECT_Y = MMIO_BASE + 0x0014;
pub const MMIO_SET_RECT_WIDTH = MMIO_BASE + 0x0018;
pub const MMIO_SET_RECT_HEIGHT = MMIO_BASE + 0x001C;


// texture commands
pub const MMIO_REQUEST_TEXTURE = MMIO_BASE + 0x0100;
pub const MMIO_TEXTURE_BUSY = MMIO_BASE + 0x0104;
pub const MMIO_TEXTURE_RESPONSE = MMIO_BASE + 0x0108;

// texture clipping rect
pub const MMIO_SET_TEXTURE_X = MMIO_BASE + 0x0110;
pub const MMIO_SET_TEXTURE_Y = MMIO_BASE + 0x0114;
pub const MMIO_SET_TEXTURE_WIDTH = MMIO_BASE + 0x0118;
pub const MMIO_SET_TEXTURE_HEIGHT = MMIO_BASE + 0x011C;

// selector commands
pub const MMIO_SET_FRAMEBUFFER_SLOT = MMIO_BASE + 0x0200;
pub const MMIO_SET_TEXTURE_SLOT = MMIO_BASE + 0x0204;
pub const MMIO_SET_COLOR = MMIO_BASE + 0x0208;


// draw commands
pub const MMIO_BLIT_SOLID_COLOR = MMIO_BASE + 0x0300;
pub const MMIO_BLIT_SOLID_COLOR_OUTLINE = MMIO_BASE + 0x0304;
pub const MMIO_BLIT_TEXTURE = MMIO_BASE + 0x0308;
pub const MMIO_BLIT_BUSY = MMIO_BASE + 0x030C;

// compositor commands
pub const MMIO_REQUEST_COMPOSITE = MMIO_BASE + 0x0400;
pub const MMIO_COMPOSITE_BUSY = MMIO_BASE + 0x0404;

// 


pub const MMIO_PRESENT_FRAMEBUFFER = MMIO_BASE + 0x1000;

pub const USER_BASE = 0x00000000;

// Helper functions
pub fn writeVolatile(comptime T: type, address: u32, value: T) void {
    @as(*volatile T, @ptrFromInt(address)).* = value;
}

pub fn readVolatile(comptime T: type, address: u32) T {
    return @as(*volatile T, @ptrFromInt(address)).*;
}

pub fn clearFramebuffer() void {
    writeVolatile(u32, MMIO_SET_RECT_X, 0);
    writeVolatile(u32, MMIO_SET_RECT_Y, 0);
    writeVolatile(u32, MMIO_SET_RECT_WIDTH, FRAMEBUFFER_WIDTH);
    writeVolatile(u32, MMIO_SET_RECT_HEIGHT, FRAMEBUFFER_HEIGHT);
    writeVolatile(u32, MMIO_SET_COLOR, 0);
    writeVolatile(u32, MMIO_BLIT_SOLID_COLOR, 0);
    while (readVolatile(u32, MMIO_BLIT_BUSY) != 0) 
    {
    }
}

pub fn blitTextureRect(texture_slot: u32, x: u32, y: u32, width: u32, height: u32) void {
    writeVolatile(u32, MMIO_SET_TEXTURE_SLOT, texture_slot);
    writeVolatile(u32, MMIO_SET_TEXTURE_X, x);
    writeVolatile(u32, MMIO_SET_TEXTURE_Y, y);
    writeVolatile(u32, MMIO_SET_TEXTURE_WIDTH, width);
    writeVolatile(u32, MMIO_SET_TEXTURE_HEIGHT, height);
    writeVolatile(u32, MMIO_BLIT_TEXTURE, 0);
    while (readVolatile(u32, MMIO_BLIT_BUSY) != 0) 
    {
    }
}

pub fn compositeFramebuffer(framebuffer_slot: u32) void {
    writeVolatile(u32, MMIO_SET_FRAMEBUFFER_SLOT, framebuffer_slot);
    writeVolatile(u32, MMIO_REQUEST_COMPOSITE, 0);
    while (readVolatile(u32, MMIO_COMPOSITE_BUSY) != 0) 
    {
    }
}

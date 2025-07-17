const std = @import("std");

const c = @cImport({
    @cInclude("windows.h");
});

const charset = @import("charset.zig");

const FONT_HEIGHT = 20;
const MATRIX_SPEED_MS = 100;
const TRAIL_LENGTH = 22;
const FADE_RATE: f32 = 0.04;

// Verify type of charset.letters at comptime
comptime {
    std.debug.assert(@TypeOf(charset.letters) == []const u16);
}

// Custom Random number generator (Linear Congruential Generator)
const Random = struct {
    state: u64,

    pub fn init(seed: u64) Random {
        return Random{ .state = seed };
    }

    // LCG parameters: same as glibc's rand()
    pub fn next(self: *Random) u32 {
        self.state = self.state *% 1103515245 +% 12345;
        return @intCast((self.state >> 16) & 0x7FFFFFFF);
    }

    pub fn intRangeLessThan(self: *Random, comptime T: type, at_least: T, less_than: T) T {
        const range = less_than - at_least;
        if (range <= 0) return at_least;
        const max = std.math.maxInt(u32);
        const bucket_size = @divFloor(max + 1, @as(u32, @intCast(range)));
        const limit = bucket_size * @as(u32, @intCast(range));
        var r: u32 = undefined;
        while (true) {
            r = self.next();
            if (r < limit) break;
        }
        return at_least + @as(T, @intCast(r / bucket_size));
    }

    pub fn float(self: *Random, comptime T: type) T {
        const max = std.math.maxInt(u32);
        return @as(T, @floatFromInt(self.next())) / @as(T, @floatFromInt(max));
    }
};

pub const Matrix = struct {
    hwnd: c.HWND,
    hfont: c.HFONT,

    width: i32,
    height: i32,
    char_width: i32,
    columns: usize,
    allocator: std.mem.Allocator,

    drops: []i32,
    trail_chars: [][]u16,
    frozen: []bool,
    frozen_trails: []std.ArrayList(FrozenTrail),
    rng: Random,

    last_update: i64,

    const FrozenTrail = struct {
        characters: [TRAIL_LENGTH]u16,
        y_pos: i32,
        alpha: f32,
    };

    // Helper function to select a random character
    fn randomChar(rng: *Random) u16 {
        return charset.letters[rng.intRangeLessThan(usize, 0, charset.letters.len)];
    }

    pub fn init(hwnd: c.HWND, width: i32, height: i32, allocator: std.mem.Allocator) !Matrix {
        const hdc = c.GetDC(hwnd);
        defer _ = c.ReleaseDC(hwnd, hdc);

        const font = c.CreateFontW(
            FONT_HEIGHT,
            0,
            0,
            0,
            c.FW_BOLD,
            0,
            0,
            0,
            c.DEFAULT_CHARSET,
            c.OUT_DEFAULT_PRECIS,
            c.CLIP_DEFAULT_PRECIS,
            c.ANTIALIASED_QUALITY,
            c.FF_MODERN,
            std.unicode.utf8ToUtf16LeStringLiteral("Consolas"),
        ) orelse return error.CreateFontFailed;

        const old_font = c.SelectObject(hdc, font);
        var tm: c.TEXTMETRICW = undefined;
        _ = c.GetTextMetricsW(hdc, &tm);
        _ = c.SelectObject(hdc, old_font);
        const char_width = tm.tmAveCharWidth;
        const columns = if (char_width > 0) @as(usize, @intCast(@divFloor(width, char_width))) else 0;

        const seed: u64 = @intCast(std.time.milliTimestamp());
        var prng = Random.init(seed);

        const drops = try allocator.alloc(i32, columns);
        const frozen = try allocator.alloc(bool, columns);
        @memset(frozen.ptr, false, frozen.len);
        const trail_chars = try allocator.alloc([]u16, columns);
        const frozen_trails = try allocator.alloc(std.ArrayList(FrozenTrail), columns);

        for (trail_chars) |*col| {
            col.* = try allocator.alloc(u16, TRAIL_LENGTH);
            for (col.*) |*ch| {
                ch.* = prng.intRangeLessThan(usize, 0, charset.letters.len);
            }
        }

        for (frozen_trails) |*list| {
            list.* = std.ArrayList(FrozenTrail).init(allocator);
        }

        for (drops) |*d| {
            d.* = -prng.intRangeLessThan(i32, 0, @divFloor(height, FONT_HEIGHT));
        }

        return Matrix{
            .hwnd = hwnd,
            .hfont = font,
            .width = width,
            .height = height,
            .char_width = char_width,
            .columns = columns,
            .allocator = allocator,
            .drops = drops,
            .frozen = frozen,
            .trail_chars = trail_chars,
            .frozen_trails = frozen_trails,
            .rng = prng,
            .last_update = std.time.milliTimestamp(),
        };
    }

    pub fn deinit(self: *Matrix) void {
        for (self.trail_chars) |trail| self.allocator.free(trail);
        for (self.frozen_trails) |*list| list.deinit();
        self.allocator.free(self.trail_chars);
        self.allocator.free(self.frozen_trails);
        self.allocator.free(self.drops);
        self.allocator.free(self.frozen);
        _ = c.DeleteObject(self.hfont);
    }

    pub fn update(self: *Matrix) void {
        const now = std.time.milliTimestamp();
        if (now - self.last_update < MATRIX_SPEED_MS) return;
        self.last_update = now;

        const screen_height_in_chars = @divFloor(self.height, FONT_HEIGHT);

        for (self.drops, self.frozen, self.trail_chars, self.frozen_trails, 0..) |*drop, *frozen, trail, *frozen_trails, i| {
            _ = i; // Explicitly use index to avoid unused capture
            if (frozen.*) {
                if (frozen_trails.items.len == 0) {
                    frozen.* = false;
                    drop.* = 0;
                }
            } else {
                var j = TRAIL_LENGTH - 1;
                while (j > 0) : (j -= 1) {
                    trail[j] = trail[j - 1];
                }
                trail[0] = randomChar(&self.rng);

                drop.* += 1;

                if (drop.* > screen_height_in_chars and self.rng.float(f32) > 0.975) {
                    frozen.* = true;
                    var frozen_trail = FrozenTrail{
                        .characters = undefined,
                        .y_pos = drop.* * FONT_HEIGHT,
                        .alpha = 1.0,
                    };
                    @memcpy(&frozen_trail.characters, trail.ptr, TRAIL_LENGTH);
                    try frozen_trails.append(frozen_trail);
                    drop.* = -self.rng.intRangeLessThan(i32, 0, screen_height_in_chars);
                }
            }

            if (self.rng.float(f32) < 0.1) {
                const idx = self.rng.intRangeLessThan(usize, 1, TRAIL_LENGTH);
                trail[idx] = randomChar(&self.rng);
            }

            var j: usize = 0;
            while (j < frozen_trails.items.len) {
                frozen_trails.items[j].alpha -= FADE_RATE;
                if (frozen_trails.items[j].alpha <= 0) {
                    _ = frozen_trails.swapRemove(j);
                } else {
                    j += 1;
                }
            }
        }
    }

    pub fn render(self: *Matrix, hdc: c.HDC) void {
        const mem_dc = c.CreateCompatibleDC(hdc);
        defer _ = c.DeleteDC(mem_dc);

        const mem_bitmap = c.CreateCompatibleBitmap(hdc, self.width, self.height);
        defer _ = c.DeleteObject(mem_bitmap);

        const old_bitmap = c.SelectObject(mem_dc, mem_bitmap);
        defer _ = c.SelectObject(mem_dc, old_bitmap);

        _ = c.SelectObject(mem_dc, self.hfont);
        _ = c.SetBkMode(mem_dc, c.TRANSPARENT);

        const brush = c.CreateSolidBrush(c.RGB(0, 0, 0));
        const rect = c.RECT{ .left = 0, .top = 0, .right = self.width, .bottom = self.height };
        _ = c.FillRect(mem_dc, &rect, brush);
        _ = c.DeleteObject(brush);

        for (self.frozen_trails) |trails| {
            const x = @as(i32, @intCast(@intFromPtr(&trails) % @as(usize, @intCast(self.width / self.char_width)))) * self.char_width;
            for (trails.items) |trail| {
                for (0..TRAIL_LENGTH) |j| {
                    const y = trail.y_pos - @as(i32, @intCast(j + 1)) * FONT_HEIGHT;
                    if (y < 0) continue;

                    const gradient_brightness: f32 = @floatFromInt(std.math.clamp(255 - (j * (255 / TRAIL_LENGTH)), 50, 255));
                    const final_brightness: u8 = @intFromFloat(std.math.clamp(gradient_brightness * trail.alpha, 0, 255));

                    if (final_brightness < 5) continue;

                    _ = c.SetTextColor(mem_dc, c.RGB(0, final_brightness, 0));
                    var ch_buf = [_]u16{ trail.characters[j], 0 };
                    _ = c.TextOutW(mem_dc, x, y, &ch_buf, 1);
                }
            }
        }

        for (self.drops, self.frozen, self.trail_chars, 0..) |drop, frozen, trail, i| {
            if (frozen) continue;

            const x = @as(i32, @intCast(i)) * self.char_width;
            const y = drop * FONT_HEIGHT;

            for (0..TRAIL_LENGTH) |j| {
                const ty = y - @as(i32, @intCast(j + 1)) * FONT_HEIGHT;
                if (ty < 0) continue;
                const brightness = std.math.clamp(255 - (j * (255 / TRAIL_LENGTH)), 50, 255);
                _ = c.SetTextColor(mem_dc, c.RGB(0, @as(u8, @intCast(brightness)), 0));
                var ch_buf = [_]u16{ trail[j], 0 };
                _ = c.TextOutW(mem_dc, x, ty, &ch_buf, 1);
            }

            _ = c.SetTextColor(mem_dc, c.RGB(200, 244, 248));
            var lead = [_]u16{ trail[0], 0 };
            _ = c.TextOutW(mem_dc, x, y, &lead, 1);
        }

        _ = c.BitBlt(hdc, 0, 0, self.width, self.height, mem_dc, 0, 0, c.SRCCOPY);
    }
};

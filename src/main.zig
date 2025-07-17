const std = @import("std");

const c = @cImport({
    @cInclude("windows.h");
});

const Matrix = @import("matrix.zig").Matrix;

var matrix_ptr: ?*Matrix = null;
var running = true;

// Windows callback function using C calling convention
pub fn wndProc(hwnd: c.HWND, msg: c.UINT, wparam: c.WPARAM, lparam: c.LPARAM) callconv(.C) c.LRESULT {
    switch (msg) {
        c.WM_DESTROY => {
            running = false;
            c.PostQuitMessage(0);
            return 0;
        },
        c.WM_KEYDOWN, c.WM_LBUTTONDOWN, c.WM_MOUSEMOVE => {
            running = false;
            c.PostQuitMessage(0);
            return 0;
        },
        c.WM_PAINT => {
            var ps: c.PAINTSTRUCT = undefined;
            const hdc = c.BeginPaint(hwnd, &ps);
            if (matrix_ptr) |matrix| {
                matrix.render(hdc);
            }
            _ = c.EndPaint(hwnd, &ps);
            return 0;
        },
        else => return c.DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}

pub fn main() !void {
    // GeneralPurposeAllocator was renamed to DebugAllocator
    var gpa = std.heap.DebugAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const hInstance = c.GetModuleHandleW(null);
    const class_name = std.unicode.utf8ToUtf16LeStringLiteral("MatrixWndClass");

    var wc: c.WNDCLASSW = std.mem.zeroes(c.WNDCLASSW);
    wc.style = c.CS_HREDRAW | c.CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = @ptrCast(@alignCast(c.GetStockObject(c.BLACK_BRUSH)));
    wc.lpszClassName = class_name;
    wc.hCursor = c.LoadCursorW(null, @as([*c]const u16, @ptrFromInt(32512))); // IDC_ARROW = 32512

    if (c.RegisterClassW(&wc) == 0) {
        std.log.err("Failed to register window class: {any}", .{c.GetLastError()});
        return error.RegisterClassFailed;
    }

    const screen_width = c.GetSystemMetrics(c.SM_CXSCREEN);
    const screen_height = c.GetSystemMetrics(c.SM_CYSCREEN);

    const hwnd = c.CreateWindowExW(
        c.WS_EX_APPWINDOW,
        class_name,
        std.unicode.utf8ToUtf16LeStringLiteral("Matrix Screensaver"),
        c.WS_POPUP,
        0,
        0,
        screen_width,
        screen_height,
        null,
        null,
        hInstance,
        null,
    );
    if (hwnd == null) {
        std.log.err("Failed to create window: {any}", .{c.GetLastError()});
        return error.CreateWindowFailed;
    }
    defer _ = c.DestroyWindow(hwnd);

    _ = c.ShowWindow(hwnd, c.SW_SHOW);
    _ = c.UpdateWindow(hwnd);

    var matrix = try Matrix.init(hwnd, screen_width, screen_height, allocator);
    defer matrix.deinit();
    matrix_ptr = &matrix;

    var msg: c.MSG = undefined;

    while (running) {
        while (c.PeekMessageW(&msg, null, 0, 0, c.PM_REMOVE) != 0) {
            if (msg.message == c.WM_QUIT) {
                running = false;
            }
            _ = c.TranslateMessage(&msg);
            _ = c.DispatchMessageW(&msg);
        }
        if (!running) break;

        matrix.update();
        _ = c.InvalidateRect(hwnd, null, false);

        std.time.sleep(10 * std.time.ns_per_ms);
    }
}

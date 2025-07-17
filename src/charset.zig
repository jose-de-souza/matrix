const std = @import("std");

pub const letters: []const u16 = blk: {
    const raw_utf8 =
        "01100011011011010011010000110101001101110011001101110010" ++
        "бвгджзклмнпрстфхцчшщаеёиоуыэюяйъь" ++
        "アイウエオカキクケコサシスセソタチツテトナニヌネノ" ++
        "ハヒフヘホマミムメモヤユヨラリルレロワンヰヱヲ";
    var buffer: [1024]u16 = undefined; // Sufficiently large buffer for the UTF-16 string
    const slice = std.unicode.utf8ToUtf16Le(&buffer, raw_utf8) catch |err| {
        @compileError("UTF-8 to UTF-16 conversion failed: " ++ @errorName(err));
    };
    break :blk slice;
};

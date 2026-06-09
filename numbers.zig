const std = @import("std");

pub fn main(init: std.process.Init) !void {    
    // 1. Allokator für die CLI-Argumente bereitstellen
    var arena: std.heap.ArenaAllocator = .init(std.heap.page_allocator);
    defer _ = arena.deinit();

    // 2. Argumente auslesen
    const args = try init.minimal.args.toSlice(arena.allocator());
    defer arena.allocator().free(args);

    // Validierung der Argumente
    if (args.len < 3) {
        std.debug.print(
            \\Missing Parameters!
            \\Benutzung: {s} <number of bits> <value>
            \\
            \\Parameter-Optionen:
            \\  <bits>        : 32, 64
            \\  <wert>        : Die umzurechnende Zahl (z.B. -42 oder 0xFF)
            \\
            \\Beispiel: {s} 64 -1
            \\
        , .{ args[0], args[0] });
        return error.InvalidArgs;
    }

    // 3. Parameter parsen
    const bits = std.fmt.parseInt(u7, args[1], 10) catch @as(u7, 64);
    var vu32:u32 = undefined;
    var vi32:i32 = undefined;
    var vu64:u64 = undefined;
    var vi64:i64 = undefined;

    const signed = args[2][0] == '-';

    switch (bits) {
        32 => {
            if (signed) {
                vi32 = try std.fmt.parseInt(i32, args[2], 0);
                vu32 = @bitCast(vi32);
            } else {
                vu32 = try std.fmt.parseInt(u32, args[2], 0);
                vi32 = @bitCast(vu32);
            }
        },
        64 => {
            if (signed) {
                vi64 = try std.fmt.parseInt(i64, args[2], 0);
                vu64 = @bitCast(vi64);
            } else {
                vu64 = try std.fmt.parseInt(u64, args[2], 0);
                vi64 = @bitCast(vu64);
            }
        },
        else => {
            std.debug.print("Number of bits must be 8, 16, 32 or 64.\n", .{});
            return;
        }
    }

    if (bits == 64) {
        std.debug.print("signed dec {}\n", .{vi64});
        std.debug.print("unsigned dec {}\n", .{vu64});
        std.debug.print("signed hex 0x{x}\n", .{vi64});
        std.debug.print("unsigned hex 0x{x}\n", .{vu64});
    }

    if (bits == 32) {
        std.debug.print("signed dec {}\n", .{vi32});
        std.debug.print("unsigned dec {}\n", .{vu32});
        std.debug.print("signed hex 0x{x}\n", .{vi32});
        std.debug.print("unsigned hex 0x{x}\n", .{vu32});
    }
}

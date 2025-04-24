const std = @import("std");
const fs = std.fs; //@import("/fs.zig");
const File = fs.File; //@import("/std/FILE.zig");

const libc = @cImport({
    @cInclude("unistd.h");
    @cInclude("string.h");
});
// build zig build-exe brightness.zig

//|-----------------|
//| main            |
//|-----------------|
pub fn main() !void {
    var brightness: u32 = 4000;

    var i = std.process.args();
    while (i.next()) |arg| {
        if (std.mem.eql(u8, arg, "hell")) {
            brightness = 19000;
        }

        if (std.mem.eql(u8, arg, "dunkel")) {
            brightness = 4000;
        }

        if (std.fmt.parseUnsigned(u32, arg, 0)) |v| {
            brightness = v;
        } else |_| {}
    }

    if (brightness > 19000) {
        brightness = 19000;
    }

    if (libc.setuid(0) == -1) {
        // const e = libc.errno;
        _ = std.c.printf("setuid(0) failed: %s\n", libc.strerror(std.c._errno().*));
        // _ = std.debug.print("setuid(0) failed: {}\n", .{std.posix.errno(-1)});
        return;
    }

    const fname = "/sys/class/backlight/intel_backlight/brightness";
    const f = fs.openFileAbsoluteZ(fname, .{ .mode = File.OpenMode.read_write, .lock = File.Lock.none }) catch |err| {
        std.debug.print(("openFileAbsoluteZ({s}) failed: {}\n"), .{ fname, err });
        return;
    };
    defer File.close(f);

    var bytes = [_]u8{ '1', '9', '0', '0', '0' };
    const b = std.fmt.bufPrint(bytes[0..], "{}", .{brightness}) catch bytes[0..];
    _ = File.write(f, b) catch |err| {
        std.debug.print(("write failed: {}\n"), .{err});
    };
}

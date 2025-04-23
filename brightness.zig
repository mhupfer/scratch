const std = @import("std");
const fs = std.fs; //@import("/fs.zig");
const File = fs.File; //@import("/std/FILE.zig");

const libc = @cImport({
    @cInclude("unistd.h");
    @cInclude("string.h");
    @cInclude("errno.h");
});
// build zig build-exe brightness.zig

//|-----------------|
//| main            |
//|-----------------|
pub fn main() !void {
    if (libc.setuid(0) == -1) {
        // const e = libc.errno;
        _ = std.c.printf("setuid(0) failed: %s\n", libc.strerror(5)); //libc.errno));
        return;
    }
    const fname = "/sys/class/backlight/intel_backlight/brightness";
    const f = fs.openFileAbsoluteZ(fname, .{ .mode = File.OpenMode.read_write, .lock = File.Lock.none }) catch |err| {
        std.debug.print(("openFileAbsoluteZ({s}) failed: {}\n"), .{ fname, err });
        return;
    };
    defer File.close(f);

    const bytes = "4000";
    _ = File.write(f, bytes) catch |err| {
        std.debug.print(("write failed: {}\n"), .{err});
    };
}

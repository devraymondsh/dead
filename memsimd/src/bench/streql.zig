const std = @import("std");
pub const memsimd = @import("memsimd");

pub fn main() !void {
    const charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    const stdout = std.io.getStdOut().writer();
    try stdout.print("Please enter the iteration number. Leave empty for default(one million).\n", .{});

    // Get the maximum_iteration from stdin
    var maximum_iterations: usize = 1_000_000;
    var stdin_buf: [20]u8 = undefined;
    const stdin = std.io.getStdIn().reader();
    if (stdin.readUntilDelimiter(&stdin_buf, '\n')) |user_input| {
        maximum_iterations = std.fmt.parseInt(usize, user_input, 10) catch 1_000_000;
    } else |_| {}

    // Setup allocators and random number generator
    var rand_impl = std.rand.DefaultPrng.init(0);
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    var arena = std.heap.ArenaAllocator.init(gpa.allocator());
    const allocator = arena.allocator();
    defer {
        arena.deinit();
        _ = gpa.deinit();
    }

    // Generate fake strings. Here are two different ArrayLists with two different
    // allocations in order to prevent the strings from being the in same memory address
    // which would ruin the whole point of this benchmark.
    var string_array1 = std.ArrayList([]u8).init(allocator);
    var string_array2 = std.ArrayList([]u8).init(allocator);
    try stdout.print("Generating random strings...\n", .{});
    for (0..maximum_iterations) |_| {
        const strlen = rand_impl.random().intRangeAtMost(usize, 2, 1024);

        var allocated_str1 = try allocator.alloc(u8, strlen + 1);
        var allocated_str2 = try allocator.alloc(u8, strlen + 1);
        for (0..strlen) |randstr_idx| {
            allocated_str1[randstr_idx] = charset[rand_impl.random().intRangeAtMost(usize, 0, charset.len - 1)];
            allocated_str2[randstr_idx] = allocated_str1[randstr_idx];
        }

        allocated_str1[allocated_str1.len - 1] = 0;
        allocated_str2[allocated_str1.len - 1] = 0;

        try string_array1.append(allocated_str1);
        try string_array2.append(allocated_str2);
    }

    const strcmp_start_time = std.time.milliTimestamp();
    for (string_array1.items, 0..) |item, idx| {
        if (std.zig.c_builtins.__builtin_strcmp(item.ptr, string_array2.items[idx].ptr) != 0) {
            std.debug.panic("Wrong comparison!\n", .{});
        }
        if (std.zig.c_builtins.__builtin_strcmp(item.ptr, "@@@@@@@@@@@@@@@@@@@@@") == 0) {
            std.debug.panic("Wrong comparison!\n", .{});
        }
    }
    const strcmp_elapsed_time = std.time.milliTimestamp() - strcmp_start_time;

    const nosimd_start_time = std.time.milliTimestamp();
    for (string_array1.items, 0..) |item, idx| {
        if (!memsimd.nosimd.eql(u8, item, string_array2.items[idx])) {
            std.debug.panic("Wrong comparison!\n", .{});
        }
        if (memsimd.nosimd.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
            std.debug.panic("Wrong comparison!\n", .{});
        }
    }
    const nosimd_elapsed_time = std.time.milliTimestamp() - nosimd_start_time;

    var sse2_elapsed_time: i64 = 0;
    if (memsimd.is_x86_64 and memsimd.sse2.check()) {
        const sse2_start_time = std.time.milliTimestamp();
        for (string_array1.items, 0..) |item, idx| {
            if (!memsimd.sse2.eql(u8, item, string_array2.items[idx])) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
            if (memsimd.sse2.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
        }
        sse2_elapsed_time = std.time.milliTimestamp() - sse2_start_time;
    } else {
        std.debug.print("SSE2 is not supported on this machine!\n", .{});
    }

    var sse42_elapsed_time: i64 = 0;
    if (memsimd.is_x86_64 and memsimd.sse42.check()) {
        const sse42_start_time = std.time.milliTimestamp();
        for (string_array1.items, 0..) |item, idx| {
            if (!memsimd.sse42.eql(u8, item, string_array2.items[idx])) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
            if (memsimd.sse42.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
        }
        sse42_elapsed_time = std.time.milliTimestamp() - sse42_start_time;
    } else {
        std.debug.print("SSE4.2 is not supported on this machine!\n", .{});
    }

    var avx2_elapsed_time: i64 = 0;
    if (memsimd.is_x86_64 and memsimd.avx2.check()) {
        const avx2_start_time = std.time.milliTimestamp();
        for (string_array1.items, 0..) |item, idx| {
            if (!memsimd.avx2.eql(u8, item, string_array2.items[idx])) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
            if (memsimd.avx2.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
        }
        avx2_elapsed_time = std.time.milliTimestamp() - avx2_start_time;
    } else {
        std.debug.print("AVX2 is not supported on this machine!\n", .{});
    }

    try stdout.print("\nC's strcmp took: {any}ms\n", .{strcmp_elapsed_time});
    try stdout.print("No SIMD strcmp took: {any}ms\n", .{nosimd_elapsed_time});
    try stdout.print("SSE2 strcmp took: {any}ms\n", .{sse2_elapsed_time});
    try stdout.print("SS4.2 strcmp took: {any}ms\n", .{sse42_elapsed_time});
    try stdout.print("AVX2 strcmp took: {any}ms\n", .{avx2_elapsed_time});

    // if (memsimd.is_x86_64 and memsimd.avx512.check()) {
    //     const avx512_start_time = std.time.milliTimestamp();
    //     for (string_array1.items, 0..) |item, idx| {
    //         if (!memsimd.avx512.eql(u8, item, string_array2.items[idx])) {
    //             std.debug.panic("Wrong comparison!\n", .{});
    //         }
    //         if (memsimd.avx512.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
    //             std.debug.panic("Wrong comparison!\n", .{});
    //         }
    //     }
    //     const avx512_elapsed_time = std.time.milliTimestamp() - avx512_start_time;
    //     try stdout.print("AVX strcmp took: {any}ms\n", .{avx512_elapsed_time});
    // }

    if (memsimd.is_aarch64) {
        var sve_elapsed_time: i64 = 0;
        const sve_start_time = std.time.milliTimestamp();
        for (string_array1.items, 0..) |item, idx| {
            if (!memsimd.sve.eql(u8, item, string_array2.items[idx])) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
            if (memsimd.sve.eql(u8, item, "@@@@@@@@@@@@@@@@@@@@@")) {
                std.debug.panic("Wrong comparison!\n", .{});
            }
        }
        sve_elapsed_time = std.time.milliTimestamp() - sve_start_time;

        try stdout.print("sve strcmp took: {any}ms\n", .{sve_elapsed_time});
    }
}

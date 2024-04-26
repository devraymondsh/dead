const std = @import("std");
const builtin = @import("builtin");
pub const memsimd = @import("memsimd");

const iteration_times = 10_000;
var rand_impl = std.rand.DefaultPrng.init(0);
const testing_types: [16]type = [16]type{
    u8,
    i8,
    u16,
    i16,
    u32,
    i32,
    u64,
    i64,
    i80,
    u80,
    i128,
    u128,
    i256,
    i256,
    f32,
    f64,
};

fn genRandomNumber(comptime T: type) T {
    if (@typeInfo(T) == .Int) {
        return rand_impl.random().int(T);
    } else if (@typeInfo(T) == .Float) {
        return rand_impl.random().float(T);
    } else {
        @compileLog("Unkown numeric type: {any}!\n", .{@typeName(T)});
        @compileError("Invalid numeric type!\n");
    }
}
fn genNumberArr(comptime T: type, allocator: std.mem.Allocator, len: usize) ![]T {
    var arr = try allocator.alloc(T, len);
    for (0..(len - 1)) |idx| {
        arr[idx] = genRandomNumber(T);
    }
    arr[len - 1] = 0;
    return arr;
}
pub fn test_function(eql: anytype, allocator: std.mem.Allocator) !void {
    inline for (testing_types) |testing_type| {
        for (0..iteration_times) |idx| {
            const number_arr = try genNumberArr(testing_type, allocator, rand_impl.random().intRangeAtMost(usize, 3, 500));
            const number_arr_dup = try allocator.dupe(testing_type, number_arr);

            // Duplicates the array and replaces an element with a random value
            const random_pos = rand_impl.random().intRangeLessThan(usize, 1, number_arr.len - 1);
            var another_number_arr = try allocator.dupe(testing_type, number_arr);
            another_number_arr[random_pos] = genRandomNumber(testing_type);
            while (number_arr[random_pos] == another_number_arr[random_pos]) {
                another_number_arr[random_pos] = genRandomNumber(testing_type);
            }

            // std.debug.print("Expecting true\n", .{});
            if (!eql(testing_type, number_arr, number_arr_dup)) {
                std.debug.print("Comparison error. Expected true but got false.\n", .{});
                std.debug.print("Left: {any}.\n", .{number_arr});
                std.debug.print("Right: {any}.\n", .{number_arr_dup});
                std.debug.panic("Iteration index: {any}\n", .{idx});
            }
            // std.debug.print("Expecting false\n", .{});
            if (eql(testing_type, number_arr, another_number_arr)) {
                std.debug.print("Comparison error. Expected false but got true.\n", .{});
                std.debug.print("Left: {any}.\n", .{number_arr});
                std.debug.print("Right: {any}.\n", .{another_number_arr});
                std.debug.print("Difference in position: {any}\n", .{random_pos});
                std.debug.panic("Iteration index: {any}\n", .{idx});
            }
        }
    }
}

test "Eql functions" {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    var arena = std.heap.ArenaAllocator.init(gpa.allocator());
    const allocator = arena.allocator();
    defer {
        arena.deinit();
        _ = gpa.deinit();
    }

    try test_function(memsimd.nosimd.eql, allocator);
    if (memsimd.is_x86_64) {
        if (memsimd.sse2.check()) {
            try test_function(memsimd.sse2.eql, allocator);
        } else {
            std.debug.print("SSE2 is not supported on this machine!\n", .{});
        }
        if (memsimd.sse42.check()) {
            try test_function(memsimd.sse42.eql, allocator);
        } else {
            std.debug.print("SSE4.2 is not supported on this machine!\n", .{});
        }
        if (memsimd.avx2.check()) {
            try test_function(memsimd.avx2.eql, allocator);
        } else {
            std.debug.print("AVX2 is not supported on this machine!\n", .{});
        }
        // if (memsimd.avx512.check()) {
        //     try test_function(memsimd.avx512.eql, allocator);
        // }
    } else if (memsimd.is_aarch64) {
        try test_function(memsimd.sve.eql, allocator);
    }
}

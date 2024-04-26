const builtin = @import("builtin");

pub const is_x86_64 = builtin.cpu.arch == .x86_64;
pub const is_aarch64 = builtin.cpu.arch == .aarch64 or builtin.cpu.arch == .aarch64_be or builtin.cpu.arch == .aarch64_32;

comptime {
    if (!is_aarch64 and !is_x86_64) {
        @compileError("memsimd only supports x86_64 and aarch64 processors!");
    }
}

pub const nosimd = @import("nosimd.zig");

pub const sve = @import("sve.zig");

pub const avx2 = @import("avx2.zig");
pub const avx512 = @import("avx512.zig");
pub const sse2 = @import("sse2.zig");
pub const sse42 = @import("sse42.zig");

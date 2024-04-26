const builtin = @import("builtin");
const common = @import("common.zig");

/// SSE4.2 support check
pub fn check() bool {
    @setRuntimeSafety(false);
    return asm (
        \\.att_syntax
        \\mov    $0, %ecx
        \\mov    $1, %eax
        \\cpuid  
        \\mov    %ecx, %eax
        \\shr    $20, %eax
        \\and    $1, %eax
        : [ret] "={eax}" (-> bool),
    );
}

const std = @import("std");

/// Equality check of a and b (a, b are bytes) using SSE4.2 instructions (16 bytes at a time) without:
/// 1: Checking the length of a and b (ensure they're equal)
/// 2: Checking if a and b point to the same location
/// 3: Checking if the length of a and b are zero
/// 4: Checking if the first elements are equal without any special instruction (for faster unsuccessful checks)
pub fn eql_byte_nocheck(a: []const u8, b: []const u8) bool {
    @setRuntimeSafety(false);

    const rem: usize = a.len & 0xf;
    const len: usize = a.len -% rem;
    const ptra = a.ptr;
    const ptrb = b.ptr;

    var off: usize = 0;
    while (off < (len + 16)) : (off +%= 16) {
        const slice_len = off - a.len;
        const res = asm (
            \\.att_syntax
            \\movdqu     (%[ptra], %[off]), %xmm0
            \\movdqu     (%[ptrb], %[off]), %xmm1
            \\mov        %[slice_len], %rax
            \\mov        %rax, %rdx
            \\pcmpestrm  $72, %xmm1, %xmm0
            \\pmovmskb   %xmm0, %rax
            : [ret] "={ax}" (-> u16),
            : [ptra] "r" (ptra),
              [off] "r" (off),
              [ptrb] "r" (ptrb),
              [slice_len] "r" (slice_len),
            : "cc"
        );
        if (res != 0b1111111111111111) {
            return false;
        }
    }

    return true;
}
/// Equality check of a and b using SSE4.2 instructions (16 bytes at a time) without:
/// 1: Checking the length of a and b (ensure they're equal)
/// 2: Checking if a and b point to the same location
/// 3: Checking if the length of a and b are zero
/// 4: Checking if the first elements are equal without any special instruction (for faster unsuccessful checks)
pub fn eql_nocheck(comptime T: type, a: []const T, b: []const T) bool {
    @setRuntimeSafety(false);
    return @call(.always_inline, eql_byte_nocheck, .{
        @as([*]const u8, @ptrCast(a.ptr))[0 .. a.len *% @sizeOf(T)],
        @as([*]const u8, @ptrCast(b.ptr))[0 .. b.len *% @sizeOf(T)],
    });
}
/// Equality check of a and b using SSE4.2 instructions (16 bytes at a time)
pub fn eql(comptime T: type, a: []const T, b: []const T) bool {
    @setRuntimeSafety(false);

    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (a.len == 0) return true;
    if (common.if_scalar_unequal(T, a, b)) return false;

    return @call(.always_inline, eql_nocheck, .{ T, a, b });
}

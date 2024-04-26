const nosimd = @import("nosimd.zig");
const builtin = @import("builtin");
const common = @import("common.zig");

/// SSE2 support check
pub fn check() bool {
    @setRuntimeSafety(false);
    return asm (
        \\.att_syntax
        \\mov    $0, %edx
        \\mov    $1, %eax
        \\cpuid  
        \\mov    %edx, %eax
        \\shr    $26, %eax
        \\and    $1, %eax
        : [ret] "={eax}" (-> bool),
    );
}

/// Equality check of a and b (a, b are bytes) using SSE2 instructions (16 bytes at a time) without:
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
    while (off < len) : (off +%= 16) {
        const res = asm (
            \\.att_syntax
            \\movups    (%[ptra], %[off]), %xmm0
            \\movups    (%[ptrb], %[off]), %xmm1
            \\pcmpeqb   %xmm1, %xmm0
            \\pmovmskb  %xmm0, %rax
            : [ret] "={ax}" (-> u16),
            : [ptra] "r" (ptra),
              [off] "r" (off),
              [ptrb] "r" (ptrb),
            : "cc"
        );
        if (res != 0b1111111111111111) {
            return false;
        }
    }
    if (rem != 0) {
        if (!nosimd.eql_nocheck(u8, a.ptr[off..a.len], b.ptr[off..b.len])) {
            return false;
        }
    }

    return true;
}
/// Equality check of a and b using SSE2 instructions (16 bytes at a time) without:
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
/// Equality check of a and b using SSE2 instructions (16 bytes at a time)
pub fn eql(comptime T: type, a: []const T, b: []const T) bool {
    @setRuntimeSafety(false);

    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (a.len == 0) return true;
    if (common.if_scalar_unequal(T, a, b)) return false;

    return @call(.always_inline, eql_nocheck, .{ T, a, b });
}

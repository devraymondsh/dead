const sse42 = @import("sse42.zig");
const builtin = @import("builtin");
const common = @import("common.zig");

/// AVX2 support check
pub fn check() bool {
    @setRuntimeSafety(false);
    return asm (
        \\.att_syntax
        \\mov    $0, %ebx
        \\mov    $0, %ecx
        \\mov    $7, %eax
        \\cpuid  
        \\mov    %ebx, %eax
        \\shr    $5, %eax
        \\and    $1, %eax
        : [ret] "={eax}" (-> bool),
    );
}

/// Equality check of a and b (a, b are bytes) using AVX2 instructions (32 bytes at a time) without:
/// 1: Checking the length of a and b (ensure they're equal)
/// 2: Checking if a and b point to the same location
/// 3: Checking if the length of a and b are zero
/// 4: Checking if the first elements are equal without any special instruction (for faster unsuccessful checks)
pub fn eql_byte_nocheck(a: []const u8, b: []const u8) bool {
    @setRuntimeSafety(false);

    const rem: usize = a.len & 0x1f;
    const len: usize = a.len -% rem;
    const ptra = a.ptr;
    const ptrb = b.ptr;

    var off: usize = 0;
    while (off < len) : (off +%= 32) {
        const res = asm (
            \\.att_syntax
            \\vmovups    (%[ptra], %[off]), %ymm0
            \\vmovups    (%[ptrb], %[off]), %ymm1
            \\vpcmpeqb   %ymm1, %ymm0, %ymm0
            \\vpmovmskb  %ymm0, %rax
            \\vzeroupper
            : [ret] "={eax}" (-> u32),
            : [ptra] "r" (ptra),
              [off] "r" (off),
              [ptrb] "r" (ptrb),
            : "cc"
        );
        if (res != 0b11111111111111111111111111111111) {
            return false;
        }
    }
    if (rem != 0) {
        if (!sse42.eql_byte_nocheck(a.ptr[off..a.len], b.ptr[off..b.len])) {
            return false;
        }
    }

    return true;
}
/// Equality check of a and b using AVX2 instructions (32 bytes at a time) without:
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
/// Equality check of a and b using AVX2 instructions (32 bytes at a time)
pub fn eql(comptime T: type, a: []const T, b: []const T) bool {
    @setRuntimeSafety(false);

    if (a.len != b.len) return false;
    if (a.ptr == b.ptr) return true;
    if (a.len == 0) return true;
    if (common.if_scalar_unequal(T, a, b)) return false;

    return @call(.always_inline, eql_nocheck, .{ T, a, b });
}

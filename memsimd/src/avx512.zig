// const sse42 = @import("sse42.zig");
// const builtin = @import("builtin");
// const common = @import("common.zig");

// /// AVX512 (AVX-512BW) support check
// pub fn check() bool {
//     @setRuntimeSafety(false);
//     return asm (
//         \\.att_syntax
//         \\mov    $0, %ebx
//         \\mov    $0, %ecx
//         \\mov    $7, %eax
//         \\cpuid
//         \\mov    %ebx, %eax
//         \\shr    $30, %eax
//         \\and    $1, %eax
//         : [ret] "={eax}" (-> bool),
//     );
// }

// extern fn _avx512_asm_eql(ptr1: [*]const u8, ptr2: [*]const u8, off: usize) bool;
// comptime {
//     asm (
//         \\.intel_syntax noprefix
//         \\
//         ++ common.underscore_prefix("avx512_asm_eql:\n") ++
//             " vmovdqu64  zmm0, zmmword ptr [" ++ common.arg1_reg ++ " + " ++ common.arg3_reg ++ "]\n" ++
//             " vmovdqu64  zmm1, zmmword ptr [" ++ common.arg2_reg ++ " + " ++ common.arg3_reg ++ "]\n" ++
//             \\ vpcmpeqb  k0, zmm0, zmm1
//             \\ kortestq k0, k0
//             \\ sete    al
//             \\ vzeroupper
//             \\ ret
//     );
// }

// /// Equality check of a and b (a, b are bytes) using AVX512 instructions (64 bytes at a time) without:
// /// 1: Checking the length of a and b (ensure they're equal)
// /// 2: Checking if a and b point to the same location
// /// 3: Checking if the length of a and b are zero
// /// 4: Checking if the first elements are equal without any special instruction (for faster unsuccessful checks)
// pub fn eql_byte_nocheck(a: []const u8, b: []const u8) bool {
//     @setRuntimeSafety(false);

//     const rem: usize = a.len & 0x3f;
//     const len: usize = a.len -% rem;

//     var off: usize = 0;
//     while (off < len) : (off +%= 64) {
//         if (!_avx512_asm_eql(a.ptr, b.ptr, off)) {
//             return false;
//         }
//     }
//     if (rem != 0) {
//         if (!sse42.eql_byte_nocheck(a.ptr[off..a.len], b.ptr[off..b.len])) {
//             return false;
//         }
//     }

//     return true;
// }
// /// Equality check of a and b using AVX512 instructions (64 bytes at a time) without:
// /// 1: Checking the length of a and b (ensure they're equal)
// /// 2: Checking if a and b point to the same location
// /// 3: Checking if the length of a and b are zero
// /// 4: Checking if the first elements are equal without any special instruction (for faster unsuccessful checks)
// pub fn eql_nocheck(comptime T: type, a: []const T, b: []const T) bool {
//     @setRuntimeSafety(false);
//     return @call(.always_inline, eql_byte_nocheck, .{
//         @as([*]const u8, @ptrCast(a.ptr))[0 .. a.len *% @sizeOf(T)],
//         @as([*]const u8, @ptrCast(b.ptr))[0 .. b.len *% @sizeOf(T)],
//     });
// }
// /// Equality check of a and b using AVX512 instructions (64 bytes at a time)
// pub fn eql(comptime T: type, a: []const T, b: []const T) bool {
//     @setRuntimeSafety(false);

//     if (a.len != b.len) return false;
//     if (a.ptr == b.ptr) return true;
//     if (a.len == 0) return true;
//     if (common.if_scalar_unequal(T, a, b)) return false;

//     return @call(.always_inline, eql_nocheck, .{ T, a, b });
// }

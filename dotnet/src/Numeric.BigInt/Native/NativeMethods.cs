using System;
using System.Runtime.InteropServices;

namespace Numeric.Native;

internal static unsafe partial class NativeMethods
{
    private const string Dll = "bigint_c";

    static NativeMethods()
    {
        NativeResolver.Initialize();
    }

    /* Lifecycle */
    [LibraryImport(Dll, EntryPoint = "bigint_create")]
    public static partial IntPtr bigint_create();

    [LibraryImport(Dll, EntryPoint = "bigint_create_i64")]
    public static partial IntPtr bigint_create_i64(long val);

    [LibraryImport(Dll, EntryPoint = "bigint_create_u64")]
    public static partial IntPtr bigint_create_u64(ulong val);

    [LibraryImport(Dll, EntryPoint = "bigint_create_str", StringMarshalling = StringMarshalling.Utf8)]
    public static partial IntPtr bigint_create_str(string str, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_clone")]
    public static partial IntPtr bigint_clone(IntPtr src);

    [LibraryImport(Dll, EntryPoint = "bigint_clone")]
    public static partial IntPtr bigint_clone(BigIntHandle src);

    [LibraryImport(Dll, EntryPoint = "bigint_destroy")]
    public static partial void bigint_destroy(IntPtr b);

    /* Zero-Heap Stack SBO */
    [LibraryImport(Dll, EntryPoint = "bigint_init_sbo256")]
    public static partial IntPtr bigint_init_sbo256(IntPtr storage, long val);

    [LibraryImport(Dll, EntryPoint = "bigint_init_sbo256_u64")]
    public static partial IntPtr bigint_init_sbo256_u64(IntPtr storage, ulong val);

    [LibraryImport(Dll, EntryPoint = "bigint_init_sbo256_str", StringMarshalling = StringMarshalling.Utf8)]
    public static partial IntPtr bigint_init_sbo256_str(IntPtr storage, string str, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_destroy_sbo256")]
    public static partial void bigint_destroy_sbo256(IntPtr storage);

    /* Assignment */
    [LibraryImport(Dll, EntryPoint = "bigint_assign")]
    public static partial int bigint_assign(IntPtr dst, IntPtr src);

    [LibraryImport(Dll, EntryPoint = "bigint_assign")]
    public static partial int bigint_assign(BigIntHandle dst, BigIntHandle src);

    [LibraryImport(Dll, EntryPoint = "bigint_set_i64")]
    public static partial int bigint_set_i64(IntPtr dst, long val);

    [LibraryImport(Dll, EntryPoint = "bigint_set_u64")]
    public static partial int bigint_set_u64(IntPtr dst, ulong val);

    [LibraryImport(Dll, EntryPoint = "bigint_set_str", StringMarshalling = StringMarshalling.Utf8)]
    public static partial int bigint_set_str(IntPtr dst, string str, int radix);

    /* Basic Arithmetic */
    [LibraryImport(Dll, EntryPoint = "bigint_add")]
    public static partial int bigint_add(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_add")]
    public static partial int bigint_add(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_sub")]
    public static partial int bigint_sub(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_sub")]
    public static partial int bigint_sub(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_mul")]
    public static partial int bigint_mul(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mul")]
    public static partial int bigint_mul(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_div")]
    public static partial int bigint_div(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_div")]
    public static partial int bigint_div(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_mod")]
    public static partial int bigint_mod(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mod")]
    public static partial int bigint_mod(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_div_rem")]
    public static partial int bigint_div_rem(IntPtr q, IntPtr r, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_div_rem")]
    public static partial int bigint_div_rem(BigIntHandle q, BigIntHandle r, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_neg")]
    public static partial int bigint_neg(IntPtr outVal, IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_neg")]
    public static partial int bigint_neg(BigIntHandle outVal, BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_abs")]
    public static partial int bigint_abs(IntPtr outVal, IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_abs")]
    public static partial int bigint_abs(BigIntHandle outVal, BigIntHandle a);

    /* In-place Arithmetic */
    [LibraryImport(Dll, EntryPoint = "bigint_add_inplace")]
    public static partial int bigint_add_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_sub_inplace")]
    public static partial int bigint_sub_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mul_inplace")]
    public static partial int bigint_mul_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_div_inplace")]
    public static partial int bigint_div_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mod_inplace")]
    public static partial int bigint_mod_inplace(IntPtr a, IntPtr b);

    /* Bitwise & Shift */
    [LibraryImport(Dll, EntryPoint = "bigint_and")]
    public static partial int bigint_and(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_and")]
    public static partial int bigint_and(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_or")]
    public static partial int bigint_or(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_or")]
    public static partial int bigint_or(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_xor")]
    public static partial int bigint_xor(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_xor")]
    public static partial int bigint_xor(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_not")]
    public static partial int bigint_not(IntPtr outVal, IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_not")]
    public static partial int bigint_not(BigIntHandle outVal, BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_shl")]
    public static partial int bigint_shl(IntPtr outVal, IntPtr a, nuint shift);

    [LibraryImport(Dll, EntryPoint = "bigint_shl")]
    public static partial int bigint_shl(BigIntHandle outVal, BigIntHandle a, nuint shift);

    [LibraryImport(Dll, EntryPoint = "bigint_shr")]
    public static partial int bigint_shr(IntPtr outVal, IntPtr a, nuint shift);

    [LibraryImport(Dll, EntryPoint = "bigint_shr")]
    public static partial int bigint_shr(BigIntHandle outVal, BigIntHandle a, nuint shift);

    /* Comparisons */
    [LibraryImport(Dll, EntryPoint = "bigint_cmp")]
    public static partial int bigint_cmp(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_cmp")]
    public static partial int bigint_cmp(BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_cmp_i64")]
    public static partial int bigint_cmp_i64(IntPtr a, long b);

    [LibraryImport(Dll, EntryPoint = "bigint_cmp_i64")]
    public static partial int bigint_cmp_i64(BigIntHandle a, long b);

    [LibraryImport(Dll, EntryPoint = "bigint_is_zero")]
    public static partial int bigint_is_zero(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_is_zero")]
    public static partial int bigint_is_zero(BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_is_negative")]
    public static partial int bigint_is_negative(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_is_negative")]
    public static partial int bigint_is_negative(BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_sign")]
    public static partial int bigint_sign(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_sign")]
    public static partial int bigint_sign(BigIntHandle a);

    /* High Precision Math */
    [LibraryImport(Dll, EntryPoint = "bigint_pow")]
    public static partial int bigint_pow(IntPtr outVal, IntPtr baseVal, uint exp);

    [LibraryImport(Dll, EntryPoint = "bigint_pow")]
    public static partial int bigint_pow(BigIntHandle outVal, BigIntHandle baseVal, uint exp);

    [LibraryImport(Dll, EntryPoint = "bigint_gcd")]
    public static partial int bigint_gcd(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_gcd")]
    public static partial int bigint_gcd(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_lcm")]
    public static partial int bigint_lcm(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_lcm")]
    public static partial int bigint_lcm(BigIntHandle outVal, BigIntHandle a, BigIntHandle b);

    [LibraryImport(Dll, EntryPoint = "bigint_isqrt")]
    public static partial int bigint_isqrt(IntPtr outVal, IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_isqrt")]
    public static partial int bigint_isqrt(BigIntHandle outVal, BigIntHandle a);

    /* Conversions */
    [LibraryImport(Dll, EntryPoint = "bigint_to_i64")]
    public static partial long bigint_to_i64(IntPtr a, out int status);

    [LibraryImport(Dll, EntryPoint = "bigint_to_i64")]
    public static partial long bigint_to_i64(BigIntHandle a, out int status);

    [LibraryImport(Dll, EntryPoint = "bigint_to_u64")]
    public static partial ulong bigint_to_u64(IntPtr a, out int status);

    [LibraryImport(Dll, EntryPoint = "bigint_to_u64")]
    public static partial ulong bigint_to_u64(BigIntHandle a, out int status);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str_len")]
    public static partial nuint bigint_to_str_len(IntPtr a, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str_len")]
    public static partial nuint bigint_to_str_len(BigIntHandle a, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str")]
    public static partial int bigint_to_str(IntPtr a, [Out] byte[] buf, nuint bufLen, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str")]
    public static partial int bigint_to_str(BigIntHandle a, [Out] byte[] buf, nuint bufLen, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str")]
    public static partial int bigint_to_str(IntPtr a, byte* buf, nuint bufLen, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str")]
    public static partial int bigint_to_str(BigIntHandle a, byte* buf, nuint bufLen, int radix);

    /* Utilities */
    [LibraryImport(Dll, EntryPoint = "bigint_bit_length")]
    public static partial nuint bigint_bit_length(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_bit_length")]
    public static partial nuint bigint_bit_length(BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_limb_count")]
    public static partial nuint bigint_limb_count(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_limb_count")]
    public static partial nuint bigint_limb_count(BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_is_sbo")]
    public static partial int bigint_is_sbo(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_is_sbo")]
    public static partial int bigint_is_sbo(BigIntHandle a);

    [LibraryImport(Dll, EntryPoint = "bigint_version")]
    public static partial IntPtr bigint_version();

    [LibraryImport(Dll, EntryPoint = "bigint_status_message")]
    public static partial IntPtr bigint_status_message(int status);
}

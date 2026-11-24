// filepath: include/numeric/detail/Intrinsics.hpp
/**
 * @file Intrinsics.hpp
 * @brief Hardware-accelerated CPU intrinsics and bitwise primitive operations for CPP-BigInt.
 *        Provides constexpr-capable adc64, sbb64, clz64, mul64_wide, div128_64, shld64, shrd64,
 *        and branchless mul_1_raw, addmul_1_raw, submul_1_raw primitives over std::span buffers.
 *        硬體加速 CPU 原語與位元基礎運算模組。
 *        提供支援 C++23 編譯期求值之進借位、前導零、乘除、雙字移位，以及基於 std::span 之單肢段乘加/乘減原語。
 */
#pragma once

#include "../Config.hpp"
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#if defined(_MSC_VER)
#  include <intrin.h>
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
#  include <immintrin.h>
#endif

namespace numeric {
namespace detail {

/// <summary>
/// Low-level hardware instruction and register primitives helper structure.
/// 底層硬體指令與暫存器原語輔助結構。
/// </summary>
struct BigIntIntrinsics {
    /// <summary>
    /// 64-bit Add-with-Carry (ADC) primitive: out = a + b + carry_in, returns carry_out (0 or 1).
    /// 64 位元帶進位加法原語：out = a + b + carry_in，回傳進位輸出 (0 或 1)。
    /// </summary>
    /// <param name="carry_in">Input carry bit (0 or 1). / 輸入進位旗標 (0 或 1)。</param>
    /// <param name="a">First 64-bit operand. / 第一個 64 位元運算元。</param>
    /// <param name="b">Second 64-bit operand. / 第二個 64 位元運算元。</param>
    /// <param name="out">Output sum reference. / 總和輸出參考。</param>
    /// <returns>Output carry flag (0 or 1). / 輸出進位旗標 (0 或 1)。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto adc64(
        const std::uint8_t carry_in,
        const std::uint64_t a,
        const std::uint64_t b,
        std::uint64_t& out) noexcept -> std::uint8_t
    {
        if (std::is_constant_evaluated()) {
            const std::uint64_t sum1{a + carry_in};
            const auto c1{static_cast<std::uint8_t>(sum1 < a)};
            const std::uint64_t sum2{sum1 + b};
            const auto c2{static_cast<std::uint8_t>(sum2 < b)};
            out = sum2;
            return static_cast<std::uint8_t>(c1 | c2);
        }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        std::uint64_t temp{0};
        const auto c{static_cast<std::uint8_t>(_addcarry_u64(carry_in, a, b, &temp))};
        out = temp;
        return c;
#elif defined(__SIZEOF_INT128__)
        const unsigned __int128 res{static_cast<unsigned __int128>(a) + b + carry_in};
        out = static_cast<std::uint64_t>(res);
        return static_cast<std::uint8_t>(res >> 64);
#else
        const std::uint64_t sum1{a + carry_in};
        const auto c1{static_cast<std::uint8_t>(sum1 < a)};
        const std::uint64_t sum2{sum1 + b};
        const auto c2{static_cast<std::uint8_t>(sum2 < b)};
        out = sum2;
        return static_cast<std::uint8_t>(c1 | c2);
#endif
    }

    /// <summary>
    /// 64-bit Subtract-with-Borrow (SBB) primitive: out = a - b - borrow_in, returns borrow_out (0 or 1).
    /// 64 位元帶借位減法原語：out = a - b - borrow_in，回傳借位輸出 (0 或 1)。
    /// </summary>
    /// <param name="borrow_in">Input borrow bit (0 or 1). / 輸入借位旗標 (0 或 1)。</param>
    /// <param name="a">Minuend operand. / 被減數。</param>
    /// <param name="b">Subtrahend operand. / 減數。</param>
    /// <param name="out">Output difference reference. / 差值輸出參考。</param>
    /// <returns>Output borrow flag (0 or 1). / 輸出借位旗標 (0 或 1)。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto sbb64(
        const std::uint8_t borrow_in,
        const std::uint64_t a,
        const std::uint64_t b,
        std::uint64_t& out) noexcept -> std::uint8_t
    {
        if (std::is_constant_evaluated()) {
            const std::uint64_t diff1{a - borrow_in};
            const auto b1{static_cast<std::uint8_t>(a < borrow_in)};
            const std::uint64_t diff2{diff1 - b};
            const auto b2{static_cast<std::uint8_t>(diff1 < b)};
            out = diff2;
            return static_cast<std::uint8_t>(b1 | b2);
        }

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        std::uint64_t temp{0};
        const auto b_out{static_cast<std::uint8_t>(_subborrow_u64(borrow_in, a, b, &temp))};
        out = temp;
        return b_out;
#elif defined(__SIZEOF_INT128__)
        const unsigned __int128 res{static_cast<unsigned __int128>(a) - b - borrow_in};
        out = static_cast<std::uint64_t>(res);
        return static_cast<std::uint8_t>((res >> 127) & 1);
#else
        const std::uint64_t diff1{a - borrow_in};
        const auto b1{static_cast<std::uint8_t>(a < borrow_in)};
        const std::uint64_t diff2{diff1 - b};
        const auto b2{static_cast<std::uint8_t>(diff1 < b)};
        out = diff2;
        return static_cast<std::uint8_t>(b1 | b2);
#endif
    }

    /// <summary>
    /// Counts leading zero bits in a 64-bit unsigned integer via C++23 std::countl_zero.
    /// 透過標準庫 std::countl_zero 計算 64 位元無符號整數之前導零數量。
    /// </summary>
    /// <param name="x">Target 64-bit unsigned integer. / 目標 64 位元無符號整數。</param>
    /// <returns>Number of leading zeros (0..64). / 前導零位元數 (0 至 64)。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto clz64(const std::uint64_t x) noexcept -> std::int32_t {
        return static_cast<std::int32_t>(std::countl_zero(x));
    }

    /// <summary>
    /// 64-bit wide multiplication: returns low 64 bits and stores high 64 bits in hi.
    /// 64 位元寬乘法運算：回傳低 64 位元乘積並將高 64 位元進位寫入 hi。
    /// </summary>
    /// <param name="a">First factor. / 乘數 a。</param>
    /// <param name="b">Second factor. / 乘數 b。</param>
    /// <param name="hi">Output reference for high 64 bits. / 高 64 位進位輸出參考。</param>
    /// <returns>Low 64 bits of the 128-bit product. / 128 位元乘積之低 64 位元。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto mul64_wide(
        const std::uint64_t a,
        const std::uint64_t b,
        std::uint64_t& hi) noexcept -> std::uint64_t
    {
        if (std::is_constant_evaluated()) {
            const std::uint64_t a_lo{static_cast<std::uint32_t>(a)};
            const std::uint64_t a_hi{a >> 32};
            const std::uint64_t b_lo{static_cast<std::uint32_t>(b)};
            const std::uint64_t b_hi{b >> 32};
            const std::uint64_t p0{a_lo * b_lo};
            const std::uint64_t p1{a_lo * b_hi};
            const std::uint64_t p2{a_hi * b_lo};
            const std::uint64_t p3{a_hi * b_hi};
            const std::uint64_t mid{p1 + static_cast<std::uint32_t>(p0 >> 32) + static_cast<std::uint32_t>(p2)};
            hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
            return (mid << 32) | static_cast<std::uint32_t>(p0);
        }
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 prod{static_cast<unsigned __int128>(a) * b};
        hi = static_cast<std::uint64_t>(prod >> 64);
        return static_cast<std::uint64_t>(prod);
#elif defined(_MSC_VER) && defined(_M_ARM64)
        hi = __umulh(a, b);
        return a * b;
#elif defined(_MSC_VER) && defined(_M_X64)
        return _umul128(a, b, &hi);
#else
        const std::uint64_t a_lo{static_cast<std::uint32_t>(a)};
        const std::uint64_t a_hi{a >> 32};
        const std::uint64_t b_lo{static_cast<std::uint32_t>(b)};
        const std::uint64_t b_hi{b >> 32};
        const std::uint64_t p0{a_lo * b_lo};
        const std::uint64_t p1{a_lo * b_hi};
        const std::uint64_t p2{a_hi * b_lo};
        const std::uint64_t p3{a_hi * b_hi};
        const std::uint64_t mid{p1 + static_cast<std::uint32_t>(p0 >> 32) + static_cast<std::uint32_t>(p2)};
        hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
        return (mid << 32) | static_cast<std::uint32_t>(p0);
#endif
    }

    /// <summary>
    /// Divides a 128-bit integer (hi:lo) by a 64-bit divisor d (Precondition: d != 0 and hi < d).
    /// 128 位元除以 64 位元整數除法（前置合約：d != 0 且 hi < d，防止觸發硬體 #DE 異常）。
    /// </summary>
    /// <param name="hi">High 64 bits of dividend. / 被除數高 64 位元。</param>
    /// <param name="lo">Low 64 bits of dividend. / 被除數低 64 位元。</param>
    /// <param name="d">Non-zero 64-bit divisor. / 非零 64 位元除數。</param>
    /// <param name="rem">Output reference for remainder. / 餘數輸出參考。</param>
    /// <returns>64-bit quotient. / 64 位元商數。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto div128_64(
        const std::uint64_t hi,
        const std::uint64_t lo,
        const std::uint64_t d,
        std::uint64_t& rem) noexcept -> std::uint64_t
    {
        assert(d != 0 && "Division by zero!");
        assert(hi < d && "Hardware divide error: quotient exceeds 64 bits!");
        if (std::is_constant_evaluated()) {
            std::uint64_t q{0};
            std::uint64_t r{hi};
            for (std::int32_t i{63}; i >= 0; --i) {
                r = (r << 1) | ((lo >> i) & 1ULL);
                if (r >= d) {
                    r -= d;
                    q |= (1ULL << i);
                }
            }
            rem = r;
            return q;
        }
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
        std::uint64_t q{0};
        std::uint64_t r{0};
        __asm__("divq %4" : "=a"(q), "=d"(r) : "d"(hi), "a"(lo), "rm"(d));
        rem = r;
        return q;
#elif defined(_MSC_VER) && defined(_M_X64)
        return _udiv128(hi, lo, d, &rem);
#elif defined(__SIZEOF_INT128__)
        const unsigned __int128 n{(static_cast<unsigned __int128>(hi) << 64) | lo};
        rem = static_cast<std::uint64_t>(n % d);
        return static_cast<std::uint64_t>(n / d);
#else
        std::uint64_t q{0};
        std::uint64_t r{hi};
        for (std::int32_t i{63}; i >= 0; --i) {
            r = (r << 1) | ((lo >> i) & 1ULL);
            if (r >= d) {
                r -= d;
                q |= (1ULL << i);
            }
        }
        rem = r;
        return q;
#endif
    }

    /// <summary>
    /// Reciprocal multiplication division of 128-bit integer by 10^19 (10000000000000000000).
    /// Eliminates _udiv128 hardware latency using precomputed inverse v = 0xd83c94fb6d2ac34a.
    /// 128 位元除以 10^19 之乘法求逆快速除法。
    /// 利用預算逆元消除 _udiv128 硬體除法指令延遲。
    /// </summary>
    /// <param name="nh">High 64 bits of dividend. / 被除數高 64 位元。</param>
    /// <param name="nl">Low 64 bits of dividend. / 被除數低 64 位元。</param>
    /// <param name="rem">Output reference for remainder. / 餘數輸出參考。</param>
    /// <returns>64-bit quotient. / 64 位元商數。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto div_recip_radix10_19(
        const std::uint64_t nh,
        const std::uint64_t nl,
        std::uint64_t& rem) noexcept -> std::uint64_t
    {
        constexpr std::uint64_t d{10000000000000000000ULL};
        constexpr std::uint64_t v{0xd83c94fb6d2ac34aULL};

        if (std::is_constant_evaluated()) {
            return div128_64(nh, nl, d, rem);
        }

        std::uint64_t nh_v_hi{0};
        const std::uint64_t nh_v_lo{mul64_wide(nh, v, nh_v_hi)};
        std::uint64_t nl_v_hi{0};
        (void)mul64_wide(nl, v, nl_v_hi);

        const std::uint64_t mid1{nh_v_lo + nl};
        const std::uint64_t carry1{(mid1 < nl) ? 1ULL : 0ULL};
        const std::uint64_t mid2{mid1 + nl_v_hi};
        const std::uint64_t carry2{(mid2 < mid1) ? 1ULL : 0ULL};

        std::uint64_t q_est{nh + nh_v_hi + carry1 + carry2};
        std::uint64_t r{nl - q_est * d};
        if (r >= d) {
            ++q_est;
            r -= d;
        }
        rem = r;
        return q_est;
    }

    /// <summary>
    /// Double-precision left funnel shift: computes high 64 bits of ((high << 64 | low) << shift).
    /// Requires 0 <= shift < 64.
    /// 雙字向左組合位移：計算 ((high << 64 | low) << shift) 的高 64 位元（要求 0 <= shift < 64）。
    /// </summary>
    /// <param name="high">High 64-bit word. / 高 64 位元字組。</param>
    /// <param name="low">Low 64-bit word. / 低 64 位元字組。</param>
    /// <param name="shift">Bit shift count (0..63). / 位移位元數 (0..63)。</param>
    /// <returns>Shifted 64-bit result. / 位移後之 64 位元結果。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto shld64(
        const std::uint64_t high,
        const std::uint64_t low,
        const std::uint32_t shift) noexcept -> std::uint64_t
    {
        assert(shift < 64U);
        if (std::is_constant_evaluated()) {
            return (shift == 0U) ? high : ((high << shift) | (low >> (64U - shift)));
        }
#if defined(_MSC_VER) && defined(_M_X64)
        return (shift == 0U) ? high : __shiftleft128(low, high, static_cast<std::uint8_t>(shift));
#else
        return (shift == 0U) ? high : ((high << shift) | (low >> (64U - shift)));
#endif
    }

    /// <summary>
    /// Double-precision right funnel shift: computes low 64 bits of ((high << 64 | low) >> shift).
    /// Requires 0 <= shift < 64.
    /// 雙字向右組合位移：計算 ((high << 64 | low) >> shift) 的低 64 位元（要求 0 <= shift < 64）。
    /// </summary>
    /// <param name="high">High 64-bit word. / 高 64 位元字組。</param>
    /// <param name="low">Low 64-bit word. / 低 64 位元字組。</param>
    /// <param name="shift">Bit shift count (0..63). / 位移位元數 (0..63)。</param>
    /// <returns>Shifted 64-bit result. / 位移後之 64 位元結果。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto shrd64(
        const std::uint64_t high,
        const std::uint64_t low,
        const std::uint32_t shift) noexcept -> std::uint64_t
    {
        assert(shift < 64U);
        if (std::is_constant_evaluated()) {
            return (shift == 0U) ? low : ((low >> shift) | (high << (64U - shift)));
        }
#if defined(_MSC_VER) && defined(_M_X64)
        return (shift == 0U) ? low : __shiftright128(low, high, static_cast<std::uint8_t>(shift));
#else
        return (shift == 0U) ? low : ((low >> shift) | (high << (64U - shift)));
#endif
    }

    /// <summary>
    /// Branchless single-limb multiplication primitive: dst[0..n-1] = src[0..n-1] * multiplier.
    /// Returns the final high-order carry limb.
    /// 無分支單肢段乘法原語：dst[0..n-1] = src[0..n-1] * multiplier，並回傳最終高位溢出進位。
    /// </summary>
    /// <param name="dst">Destination span (size >= src.size()). / 目標輸出 span（長度須 >= src.size()）。</param>
    /// <param name="src">Source limb span. / 來源運算元 span。</param>
    /// <param name="multiplier">64-bit scalar multiplier. / 64 位元純量乘數。</param>
    /// <returns>Final overflow carry limb. / 最終高位溢出進位。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto mul_1_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src,
        const std::uint64_t multiplier) noexcept -> std::uint64_t
    {
        const std::size_t n{src.size()};
        assert(dst.size() >= n);

        if (std::is_constant_evaluated()) {
            std::uint64_t carry{0};
            for (std::size_t i{0}; i < n; ++i) {
                std::uint64_t hi{0};
                const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
                std::uint64_t res_limb{0};
                const std::uint8_t c{adc64(0, lo, carry, res_limb)};
                dst[i] = res_limb;
                carry = hi + c;
            }
            return carry;
        }
#if defined(__SIZEOF_INT128__)
        unsigned __int128 carry{0};
        for (std::size_t i{0}; i < n; ++i) {
            const unsigned __int128 prod{static_cast<unsigned __int128>(src[i]) * multiplier + carry};
            dst[i] = static_cast<std::uint64_t>(prod);
            carry = prod >> 64;
        }
        return static_cast<std::uint64_t>(carry);
#elif defined(_MSC_VER) && defined(_M_X64)
        std::uint64_t carry{0};
        std::uint64_t* NUMERIC_RESTRICT d{dst.data()};
        const std::uint64_t* NUMERIC_RESTRICT s{src.data()};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            const std::uint64_t lo{_umul128(s[i], multiplier, &hi)};
            std::uint64_t res_limb{0};
            const std::uint8_t c{_addcarry_u64(0, lo, carry, &res_limb)};
            d[i] = res_limb;
            carry = hi + c;
        }
        return carry;
#else
        std::uint64_t carry{0};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
            std::uint64_t res_limb{0};
            const std::uint8_t c{adc64(0, lo, carry, res_limb)};
            dst[i] = res_limb;
            carry = hi + c;
        }
        return carry;
#endif
    }

    /// <summary>
    /// Branchless multiply-accumulate primitive: dst[0..n-1] += src[0..n-1] * multiplier.
    /// Returns the final high-order carry limb.
    /// 無分支單肢段乘加原語：dst[0..n-1] += src[0..n-1] * multiplier，回傳最終高位進位。
    /// </summary>
    /// <param name="dst">Target accumulator span (size >= src.size()). / 目標累加輸出 span。</param>
    /// <param name="src">Source limb span. / 來源運算元 span。</param>
    /// <param name="multiplier">64-bit scalar multiplier. / 64 位元純量乘數。</param>
    /// <returns>Final overflow carry limb. / 最終高位溢出進位。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto addmul_1_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src,
        const std::uint64_t multiplier) noexcept -> std::uint64_t
    {
        const std::size_t n{src.size()};
        assert(dst.size() >= n);

        if (std::is_constant_evaluated()) {
            std::uint64_t carry{0};
            for (std::size_t i{0}; i < n; ++i) {
                std::uint64_t hi{0};
                const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
                std::uint64_t sum1{0};
                const std::uint8_t c1{adc64(0, dst[i], lo, sum1)};
                std::uint64_t sum2{0};
                const std::uint8_t c2{adc64(0, sum1, carry, sum2)};
                dst[i] = sum2;
                carry = hi + c1 + c2;
            }
            return carry;
        }
#if defined(__SIZEOF_INT128__)
        unsigned __int128 carry{0};
        for (std::size_t i{0}; i < n; ++i) {
            const unsigned __int128 prod{static_cast<unsigned __int128>(src[i]) * multiplier + dst[i] + carry};
            dst[i] = static_cast<std::uint64_t>(prod);
            carry = prod >> 64;
        }
        return static_cast<std::uint64_t>(carry);
#elif defined(_MSC_VER) && defined(_M_X64)
        std::uint64_t carry{0};
        std::uint64_t* NUMERIC_RESTRICT d{dst.data()};
        const std::uint64_t* NUMERIC_RESTRICT s{src.data()};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            std::uint64_t lo{_umul128(s[i], multiplier, &hi)};
            const std::uint8_t c1{_addcarry_u64(0, lo, carry, &lo)};
            hi += c1;
            std::uint64_t sum_limb{0};
            const std::uint8_t c2{_addcarry_u64(0, d[i], lo, &sum_limb)};
            d[i] = sum_limb;
            carry = hi + c2;
        }
        return carry;
#else
        std::uint64_t carry{0};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
            std::uint64_t sum1{0};
            const std::uint8_t c1{adc64(0, dst[i], lo, sum1)};
            std::uint64_t sum2{0};
            const std::uint8_t c2{adc64(0, sum1, carry, sum2)};
            dst[i] = sum2;
            carry = hi + c1 + c2;
        }
        return carry;
#endif
    }

    /// <summary>
    /// Branchless multiply-subtract primitive: dst[0..n-1] -= src[0..n-1] * multiplier.
    /// Returns the final high-order borrow limb.
    /// 無分支單肢段乘減原語：dst[0..n-1] -= src[0..n-1] * multiplier，回傳最終高位借位。
    /// </summary>
    /// <param name="dst">Target accumulator span (size >= src.size()). / 目標減數累加 span。</param>
    /// <param name="src">Source limb span. / 來源運算元 span。</param>
    /// <param name="multiplier">64-bit scalar multiplier. / 64 位元純量乘數。</param>
    /// <returns>Final overflow borrow limb. / 最終高位溢出借位。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto submul_1_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src,
        const std::uint64_t multiplier) noexcept -> std::uint64_t
    {
        const std::size_t n{src.size()};
        assert(dst.size() >= n);

        if (std::is_constant_evaluated()) {
            std::uint64_t borrow{0};
            for (std::size_t i{0}; i < n; ++i) {
                std::uint64_t hi{0};
                const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
                std::uint64_t diff1{0};
                const std::uint8_t b1{sbb64(0, dst[i], lo, diff1)};
                std::uint64_t diff2{0};
                const std::uint8_t b2{sbb64(0, diff1, borrow, diff2)};
                dst[i] = diff2;
                borrow = hi + b1 + b2;
            }
            return borrow;
        }
#if defined(__SIZEOF_INT128__)
        std::uint64_t borrow{0};
        for (std::size_t i{0}; i < n; ++i) {
            const unsigned __int128 prod{static_cast<unsigned __int128>(src[i]) * multiplier + borrow};
            const auto lo{static_cast<std::uint64_t>(prod)};
            const auto hi{static_cast<std::uint64_t>(prod >> 64)};
            const std::uint64_t cur{dst[i]};
            dst[i] = cur - lo;
            borrow = hi + (cur < lo ? 1ULL : 0ULL);
        }
        return borrow;
#elif defined(_MSC_VER) && defined(_M_X64)
        std::uint64_t borrow{0};
        std::uint64_t* NUMERIC_RESTRICT d{dst.data()};
        const std::uint64_t* NUMERIC_RESTRICT s{src.data()};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            std::uint64_t lo{_umul128(s[i], multiplier, &hi)};
            const std::uint8_t c{_addcarry_u64(0, lo, borrow, &lo)};
            hi += c;
            std::uint64_t diff_limb{0};
            const std::uint8_t b{_subborrow_u64(0, d[i], lo, &diff_limb)};
            d[i] = diff_limb;
            borrow = hi + b;
        }
        return borrow;
#else
        std::uint64_t borrow{0};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t hi{0};
            const std::uint64_t lo{mul64_wide(src[i], multiplier, hi)};
            std::uint64_t diff1{0};
            const std::uint8_t b1{sbb64(0, dst[i], lo, diff1)};
            std::uint64_t diff2{0};
            const std::uint8_t b2{sbb64(0, diff1, borrow, diff2)};
            dst[i] = diff2;
            borrow = hi + b1 + b2;
        }
        return borrow;
#endif
    }
};

} // namespace detail
} // namespace numeric

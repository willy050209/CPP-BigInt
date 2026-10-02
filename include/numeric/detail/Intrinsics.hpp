#pragma once

// Intrinsics.hpp
// Hardware-accelerated CPU intrinsics and bitwise primitive operations for CPP-BigInt.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "../Config.hpp"
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <type_traits>

#if defined(_MSC_VER)
#  include <intrin.h>
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
#  include <immintrin.h>
#endif

namespace numeric {
namespace detail {

/// <summary>
/// 底層硬體指令與暫存器原語輔助結構。
/// </summary>
struct BigIntIntrinsics {
    /// <summary>
    /// 64-bit ADC 原語：out = a + b + carry_in，回傳 carry_out (0 或 1)。
    /// 消除 C++ 純量條件判斷分支，並支援 C++20 constexpr 常數求值。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint8_t adc64(
        uint8_t carry_in, uint64_t a, uint64_t b, uint64_t* out) noexcept
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t sum = a + carry_in;
            uint8_t c1 = static_cast<uint8_t>(sum < a);
            sum += b;
            uint8_t c2 = static_cast<uint8_t>(sum < b);
            *out = sum;
            return c1 | c2;
        }
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned __int64 temp = 0;
        uint8_t c = _addcarry_u64(carry_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return c;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
        unsigned long long temp = 0;
        uint8_t c = _addcarry_u64(carry_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return c;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(a) + b + carry_in;
        *out = static_cast<uint64_t>(res);
        return static_cast<uint8_t>(res >> 64);
#else
        uint64_t sum = a + carry_in;
        uint8_t c1 = static_cast<uint8_t>(sum < a);
        sum += b;
        uint8_t c2 = static_cast<uint8_t>(sum < b);
        *out = sum;
        return c1 | c2;
#endif
    }

    /// <summary>
    /// 64-bit SBB 原語：out = a - b - borrow_in，回傳 borrow_out (0 或 1)。
    /// 消除 C++ 純量條件判斷分支，並支援 C++20 constexpr 常數求值。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint8_t sbb64(
        uint8_t borrow_in, uint64_t a, uint64_t b, uint64_t* out) noexcept
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t diff = a - borrow_in;
            uint8_t b1 = static_cast<uint8_t>(a < borrow_in);
            uint64_t diff2 = diff - b;
            uint8_t b2 = static_cast<uint8_t>(diff < b);
            *out = diff2;
            return b1 | b2;
        }
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned __int64 temp = 0;
        uint8_t b_out = _subborrow_u64(borrow_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return b_out;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
        unsigned long long temp = 0;
        uint8_t b_out = _subborrow_u64(borrow_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return b_out;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(a) - b - borrow_in;
        *out = static_cast<uint64_t>(res);
        return static_cast<uint8_t>((res >> 127) & 1);
#else
        uint64_t diff = a - borrow_in;
        uint8_t b1 = static_cast<uint8_t>(a < borrow_in);
        uint64_t diff2 = diff - b;
        uint8_t b2 = static_cast<uint8_t>(diff < b);
        *out = diff2;
        return b1 | b2;
#endif
    }

    /// <summary>
    /// 計算 64 位元整數之前導 0 數量 (Count Leading Zeros)。
    /// </summary>
    /// <param name="x">目標 64 位元整數</param>
    /// <returns>前導 0 數量 (0~64)</returns>
    static NUMERIC_CONSTEXPR_20 int clz64(uint64_t x) noexcept {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            if (x == 0) return 64;
            int n = 0;
            if ((x >> 32) == 0) { n += 32; x <<= 32; }
            if ((x >> 48) == 0) { n += 16; x <<= 16; }
            if ((x >> 56) == 0) { n += 8;  x <<= 8;  }
            if ((x >> 60) == 0) { n += 4;  x <<= 4;  }
            if ((x >> 62) == 0) { n += 2;  x <<= 2;  }
            if ((x >> 63) == 0) { n += 1; }
            return n;
        }
#endif
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned long idx;
        if (_BitScanReverse64(&idx, x)) {
            return static_cast<int>(63 - idx);
        }
        return 64;
#elif defined(__GNUC__) || defined(__clang__)
        return (x == 0) ? 64 : __builtin_clzll(x);
#else
        if (x == 0) return 64;
        int n = 0;
        if ((x >> 32) == 0) { n += 32; x <<= 32; }
        if ((x >> 48) == 0) { n += 16; x <<= 16; }
        if ((x >> 56) == 0) { n += 8;  x <<= 8;  }
        if ((x >> 60) == 0) { n += 4;  x <<= 4;  }
        if ((x >> 62) == 0) { n += 2;  x <<= 2;  }
        if ((x >> 63) == 0) { n += 1; }
        return n;
#endif
    }

    /// <summary>
    /// 64 位元乘法運算，輸出低 64 位並回傳高 64 位進位。
    /// </summary>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    /// <param name="hi">輸出高 64 位進位參考</param>
    /// <returns>乘積之低 64 位元</returns>
    static NUMERIC_CONSTEXPR_20 uint64_t mul64_wide(uint64_t a, uint64_t b, uint64_t& hi) noexcept {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t a_lo = static_cast<uint32_t>(a);
            uint64_t a_hi = a >> 32;
            uint64_t b_lo = static_cast<uint32_t>(b);
            uint64_t b_hi = b >> 32;
            uint64_t p0 = a_lo * b_lo;
            uint64_t p1 = a_lo * b_hi;
            uint64_t p2 = a_hi * b_lo;
            uint64_t p3 = a_hi * b_hi;
            uint64_t mid = p1 + static_cast<uint32_t>(p0 >> 32) + static_cast<uint32_t>(p2);
            hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
            return (mid << 32) | static_cast<uint32_t>(p0);
        }
#endif
#if defined(__SIZEOF_INT128__)
        unsigned __int128 prod = static_cast<unsigned __int128>(a) * b;
        hi = static_cast<uint64_t>(prod >> 64);
        return static_cast<uint64_t>(prod);
#elif defined(_MSC_VER) && defined(_M_ARM64)
        hi = __umulh(a, b);
        return a * b;
#elif defined(_MSC_VER) && defined(_M_X64)
        return _umul128(a, b, &hi);
#else
        uint64_t a_lo = static_cast<uint32_t>(a);
        uint64_t a_hi = a >> 32;
        uint64_t b_lo = static_cast<uint32_t>(b);
        uint64_t b_hi = b >> 32;
        uint64_t p0 = a_lo * b_lo;
        uint64_t p1 = a_lo * b_hi;
        uint64_t p2 = a_hi * b_lo;
        uint64_t p3 = a_hi * b_hi;
        uint64_t mid = p1 + static_cast<uint32_t>(p0 >> 32) + static_cast<uint32_t>(p2);
        hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
        return (mid << 32) | static_cast<uint32_t>(p0);
#endif
    }

    /// <summary>
    /// 128 位元除以 64 位元整數之除法（前置合約：d != 0 且 hi 小於 d，防止觸發硬體 #DE 異常中斷）。
    /// </summary>
    /// <param name="hi">被除數高 64 位元</param>
    /// <param name="lo">被除數低 64 位元</param>
    /// <param name="d">除數</param>
    /// <param name="rem">輸出餘數參考</param>
    /// <returns>商之 64 位元數值</returns>
    static NUMERIC_CONSTEXPR_20 uint64_t div128_64(uint64_t hi, uint64_t lo, uint64_t d, uint64_t& rem) noexcept {
        assert(d != 0 && "Division by zero!");
        assert(hi < d && "Hardware divide error: quotient exceeds 64 bits!");
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t q = 0;
            uint64_t r = hi;
            for (int i = 63; i >= 0; --i) {
                r = (r << 1) | ((lo >> i) & 1);
                if (r >= d) {
                    r -= d;
                    q |= (1ULL << i);
                }
            }
            rem = r;
            return q;
        }
#endif
#if defined(__SIZEOF_INT128__)
        unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        rem = static_cast<uint64_t>(n % d);
        return static_cast<uint64_t>(n / d);
#elif defined(_MSC_VER) && defined(_M_X64)
        return _udiv128(hi, lo, d, &rem);
#else
        uint64_t q = 0;
        uint64_t r = hi;
        for (int i = 63; i >= 0; --i) {
            r = (r << 1) | ((lo >> i) & 1);
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
    /// 128 位元除以 10^19 (10000000000000000000) 之乘法求逆除法。
    /// 預算逆元常數 v = floor((2^128 - 1) / 10^19) - 2^64 = 0xd83c94fb6d2ac34a。
    /// 消除 _udiv128 硬體除法指令，將除法延遲自 ~40 cycles 降低至 ~6 cycles。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint64_t div_recip_radix10_19(
        uint64_t nh, uint64_t nl, uint64_t& rem) noexcept
    {
        constexpr uint64_t d = 10000000000000000000ULL;
        constexpr uint64_t v = 0xd83c94fb6d2ac34aULL;

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            return div128_64(nh, nl, d, rem);
        }
#endif

        uint64_t nh_v_hi = 0;
        uint64_t nh_v_lo = mul64_wide(nh, v, nh_v_hi);
        uint64_t nl_v_hi = 0;
        mul64_wide(nl, v, nl_v_hi);

        uint64_t mid1 = nh_v_lo + nl;
        uint64_t carry1 = (mid1 < nl) ? 1 : 0;
        uint64_t mid2 = mid1 + nl_v_hi;
        uint64_t carry2 = (mid2 < mid1) ? 1 : 0;

        uint64_t q_est = nh + nh_v_hi + carry1 + carry2;
        uint64_t r = nl - q_est * d;
        if (r >= d) {
            ++q_est;
            r -= d;
        }
        rem = r;
        return q_est;
    }
};

} // namespace detail
} // namespace numeric

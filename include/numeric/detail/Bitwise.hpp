// filepath: include/numeric/detail/Bitwise.hpp
/**
 * @file Bitwise.hpp
 * @brief Arbitrary-precision bitwise logic and hardware-accelerated shift operations for CPP-BigInt.
 *        Features branchless shld64/shrd64 funnels, two-phase vectorized in-place AND/OR/XOR,
 *        zero-allocation two's-complement NOT, and bounds-safe std::span slicing.
 *        任意精度位元邏輯與硬體加速移位運算模組。
 *        採用無分支 shld64/shrd64 雙字移位、兩階段向量化就地位元運算與零配置二補數 NOT。
 */
#pragma once

#include "Arithmetic.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace numeric {
namespace detail {

/// <summary>
/// Arbitrary-precision bitwise logic and shifting algorithms inheriting BigIntArithmetic.
/// 位元邏輯與位移演算法類別，繼承自 BigIntArithmetic，支援完整二補數邏輯與移位運算。
/// </summary>
struct BigIntBitwise : public BigIntArithmetic {
    /// <summary>
    /// High-speed whole-limb left shift: res = a << (limbs * 64).
    /// limbs 級別的高效整區塊左移：res = a << (limbs * 64)。
    /// </summary>
    /// <param name="res">Output storage. / 輸出結果儲存體。</param>
    /// <param name="a">Input storage. / 輸入數值儲存體。</param>
    /// <param name="limbs">Number of 64-bit limbs to shift. / 移動之 limb 數量。</param>
    static NUMERIC_CONSTEXPR_20 auto shift_left_limbs(
        BigIntStorage& res,
        const BigIntStorage& a,
        const std::size_t limbs) -> void
    {
        if (a.m_size == 0U) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (&res == &a) {
            BigIntStorage tmp;
            shift_left_limbs(tmp, a, limbs);
            res = std::move(tmp);
            return;
        }
        res.resize(a.m_size + limbs, 0ULL);
        BigIntStorage::copy_limbs(res.as_span().subspan(limbs), a.as_span());
        BigIntStorage::zero_limbs(res.as_span().subspan(0U, limbs));
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// Bitwise left shift: res = a << shift, eliminating inner branch stalls via shld64.
    /// 位元左移運算：res = a << shift，透過 shld64 硬體指令消除迴圈內分支與多餘運算。
    /// </summary>
    /// <param name="res">Output storage. / 輸出結果儲存體。</param>
    /// <param name="a">Input operand storage. / 運算元儲存體。</param>
    /// <param name="shift">Bit shift count. / 位移位元數。</param>
    static NUMERIC_CONSTEXPR_20 auto shift_left(
        BigIntStorage& res,
        const BigIntStorage& a,
        const std::size_t shift) -> void
    {
        if (shift == 0U || a.m_size == 0U) {
            res = a;
            return;
        }
        const std::size_t limb_shift{shift / 64U};
        const std::size_t bit_shift{shift % 64U};
        const bool overflow{(bit_shift != 0U) && ((a.as_span()[a.m_size - 1U] >> (64U - bit_shift)) != 0ULL)};
        const std::size_t new_size{a.m_size + limb_shift + (overflow ? 1U : 0U)};

        if (&res == &a) {
            const std::size_t old_size{a.m_size};
            if (res.m_capacity < new_size) {
                res.reserve(new_size);
            }
            res.m_size = static_cast<std::uint32_t>(new_size);
            const std::span<std::uint64_t> d{res.as_span()};

            if (limb_shift == 0U) {
                const auto bshift{static_cast<std::uint32_t>(bit_shift)};
                if (old_size == 1U) {
                    const std::uint64_t v0{d[0]};
                    if (overflow) d[1] = v0 >> (64U - bit_shift);
                    d[0] = v0 << bshift;
                    return;
                }
                if (old_size == 2U) {
                    const std::uint64_t v0{d[0]};
                    const std::uint64_t v1{d[1]};
                    if (overflow) d[2] = v1 >> (64U - bit_shift);
                    d[1] = shld64(v1, v0, bshift);
                    d[0] = v0 << bshift;
                    return;
                }
                if (old_size == 4U) {
                    const std::uint64_t v0{d[0]};
                    const std::uint64_t v1{d[1]};
                    const std::uint64_t v2{d[2]};
                    const std::uint64_t v3{d[3]};
                    if (overflow) d[4] = v3 >> (64U - bit_shift);
                    d[3] = shld64(v3, v2, bshift);
                    d[2] = shld64(v2, v1, bshift);
                    d[1] = shld64(v1, v0, bshift);
                    d[0] = v0 << bshift;
                    return;
                }
                if (overflow) {
                    d[old_size] = d[old_size - 1U] >> (64U - bit_shift);
                }
                for (std::size_t i{old_size - 1U}; i > 0U; --i) {
                    d[i] = shld64(d[i], d[i - 1U], bshift);
                }
                d[0] <<= bshift;
                return;
            }

            if (bit_shift == 0U) {
                for (std::size_t i{old_size}; i > 0U; --i) {
                    d[i - 1U + limb_shift] = d[i - 1U];
                }
                BigIntStorage::zero_limbs(d.subspan(0U, limb_shift));
            } else {
                const auto bshift{static_cast<std::uint32_t>(bit_shift)};
                if (overflow) {
                    d[old_size + limb_shift] = d[old_size - 1U] >> (64U - bit_shift);
                }
                for (std::size_t i{old_size - 1U}; i > 0U; --i) {
                    d[i + limb_shift] = shld64(d[i], d[i - 1U], bshift);
                }
                d[limb_shift] = d[0] << bshift;
                BigIntStorage::zero_limbs(d.subspan(0U, limb_shift));
            }
            return;
        }

        res.resize(new_size, 0ULL);
        const std::span<std::uint64_t> out{res.as_span()};
        const std::span<const std::uint64_t> src{a.as_span()};

        if (bit_shift == 0U) {
            BigIntStorage::copy_limbs(out.subspan(limb_shift), src);
            if (limb_shift > 0U) {
                BigIntStorage::zero_limbs(out.subspan(0U, limb_shift));
            }
        } else {
            if (limb_shift > 0U) {
                BigIntStorage::zero_limbs(out.subspan(0U, limb_shift));
            }
            const auto bshift{static_cast<std::uint32_t>(bit_shift)};
            std::uint64_t prev{0};
            for (std::size_t i{0}; i < src.size(); ++i) {
                const std::uint64_t cur{src[i]};
                out[i + limb_shift] = shld64(cur, prev, bshift);
                prev = cur;
            }
            if (overflow) {
                out[src.size() + limb_shift] = prev >> (64U - bit_shift);
            }
        }
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// Arithmetic bitwise right shift: res = a >> shift (supports signed two's complement semantics).
    /// 位元右移運算：res = a >> shift（支援負數算術右移二補數語意）。
    /// </summary>
    /// <param name="res">Output storage. / 輸出結果儲存體。</param>
    /// <param name="a">Input operand storage. / 運算元儲存體。</param>
    /// <param name="shift">Bit shift count. / 位移位元數。</param>
    static NUMERIC_CONSTEXPR_20 auto shift_right(
        BigIntStorage& res,
        const BigIntStorage& a,
        const std::size_t shift) -> void
    {
        if (shift == 0U || a.m_size == 0U) {
            res = a;
            return;
        }
        if (a.m_sign < 0) {
            BigIntStorage u;
            BigIntStorage shifted_u;
            bitwise_not(u, a);
            shift_right_positive(shifted_u, u, shift);
            bitwise_not(res, shifted_u);
            return;
        }
        shift_right_positive(res, a, shift);
    }

    /// <summary>
    /// In-place unsigned bitwise right shift using shrd64 and branchless forward sweeps.
    /// 正整數無符號位元右移運算：透過 shrd64 硬體指令與原地前向搬移消除迴圈內分支。
    /// </summary>
    /// <param name="res">Output storage. / 輸出結果儲存體。</param>
    /// <param name="a">Input operand storage. / 運算元儲存體。</param>
    /// <param name="shift">Bit shift count. / 位移位元數。</param>
    static NUMERIC_CONSTEXPR_20 auto shift_right_positive(
        BigIntStorage& res,
        const BigIntStorage& a,
        const std::size_t shift) -> void
    {
        const std::size_t limb_shift{shift / 64U};
        const std::size_t bit_shift{shift % 64U};
        const std::size_t old_size{a.m_size};
        if (limb_shift >= old_size) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        std::size_t new_size{old_size - limb_shift};

        if (&res == &a) {
            const std::span<std::uint64_t> d{res.as_span()};
            const auto bshift{static_cast<std::uint32_t>(bit_shift)};
            if (limb_shift == 0U) {
                if (old_size == 1U) {
                    d[0] >>= bshift;
                    res.m_size = (d[0] != 0ULL) ? 1U : 0U;
                    if (res.m_size == 0U) res.m_sign = 0;
                    return;
                }
                if (old_size == 2U) {
                    const std::uint64_t v0{d[0]};
                    const std::uint64_t v1{d[1]};
                    d[0] = shrd64(v1, v0, bshift);
                    const std::uint64_t top{v1 >> bshift};
                    d[1] = top;
                    res.m_size = (top != 0ULL) ? 2U : ((d[0] != 0ULL) ? 1U : 0U);
                    if (res.m_size == 0U) res.m_sign = 0;
                    return;
                }
                if (old_size == 4U) {
                    const std::uint64_t v0{d[0]};
                    const std::uint64_t v1{d[1]};
                    const std::uint64_t v2{d[2]};
                    const std::uint64_t v3{d[3]};
                    d[0] = shrd64(v1, v0, bshift);
                    d[1] = shrd64(v2, v1, bshift);
                    d[2] = shrd64(v3, v2, bshift);
                    const std::uint64_t top{v3 >> bshift};
                    d[3] = top;
                    std::size_t sz{4U};
                    while (sz > 0U && d[sz - 1U] == 0ULL) {
                        --sz;
                    }
                    res.m_size = static_cast<std::uint32_t>(sz);
                    if (sz == 0U) res.m_sign = 0;
                    return;
                }
            }
            if (bit_shift == 0U) {
                if (limb_shift > 0U) {
                    for (std::size_t i{0}; i < new_size; ++i) {
                        d[i] = d[i + limb_shift];
                    }
                }
            } else {
                for (std::size_t i{0}; i + 1U < new_size; ++i) {
                    d[i] = shrd64(d[i + limb_shift + 1U], d[i + limb_shift], bshift);
                }
                d[new_size - 1U] = d[old_size - 1U] >> bshift;
            }
            if (new_size > 0U && d[new_size - 1U] == 0ULL) {
                --new_size;
            }
            res.m_size = static_cast<std::uint32_t>(new_size);
            if (new_size == 0U) {
                res.m_sign = 0;
            }
            res.shrink_to_sbo_if_possible();
            return;
        }

        res.resize(new_size, 0ULL);
        const std::span<std::uint64_t> out{res.as_span()};
        const std::span<const std::uint64_t> src{a.as_span().subspan(limb_shift)};

        if (bit_shift == 0U) {
            BigIntStorage::copy_limbs(out, src);
        } else {
            const auto bshift{static_cast<std::uint32_t>(bit_shift)};
            for (std::size_t i{0}; i + 1U < new_size; ++i) {
                out[i] = shrd64(src[i + 1U], src[i], bshift);
            }
            out[new_size - 1U] = src[new_size - 1U] >> bshift;
        }
        res.m_sign = (new_size > 0U) ? a.m_sign : 0;
        res.normalize();
    }

    /// <summary>
    /// Two's complement bitwise NOT: ~a = -a - 1 with early-exit block copy.
    /// 位元非運算：~a = -a - 1，採快速進/借位截斷技術直接區塊拷貝剩餘高階肢段。
    /// </summary>
    /// <param name="res">Output storage. / 輸出結果儲存體。</param>
    /// <param name="a">Input operand storage. / 運算元儲存體。</param>
    static NUMERIC_CONSTEXPR_20 auto bitwise_not(BigIntStorage& res, const BigIntStorage& a) -> void {
        if (a.m_size == 0U || a.m_sign == 0) {
            res.set_uint64(1ULL, -1);
            return;
        }
        const std::span<const std::uint64_t> a_span{a.as_span()};
        if (a.m_sign > 0) {
            std::size_t k{0};
            while (k < a.m_size && a_span[k] == 0xFFFFFFFFFFFFFFFFULL) {
                ++k;
            }
            const std::size_t new_size{(k == a.m_size) ? (a.m_size + 1U) : a.m_size};

            if (&res == &a) {
                if (res.m_capacity < new_size) {
                    res.reserve(new_size);
                }
                const std::span<std::uint64_t> out{res.capacity_span()};
                BigIntStorage::zero_limbs(out.subspan(0U, k));
                if (k < a.m_size) {
                    out[k] += 1ULL;
                } else {
                    out[a.m_size] = 1ULL;
                }
                res.m_size = static_cast<std::uint32_t>(new_size);
                res.m_sign = -1;
                res.normalize();
                return;
            }

            res.resize(new_size, 0ULL);
            const std::span<std::uint64_t> out{res.as_span()};
            BigIntStorage::zero_limbs(out.subspan(0U, k));
            if (k < a.m_size) {
                out[k] = a_span[k] + 1ULL;
                if (a.m_size > k + 1U) {
                    BigIntStorage::copy_limbs(out.subspan(k + 1U), a_span.subspan(k + 1U));
                }
            } else {
                out[a.m_size] = 1ULL;
            }
            res.m_sign = -1;
            res.normalize();
            return;
        }

        // Negative case: a = -U => ~a = U - 1 / 負數情況：~(-U) = U - 1
        std::size_t k{0};
        while (k < a.m_size && a_span[k] == 0ULL) {
            ++k;
        }
        if (&res == &a) {
            const std::span<std::uint64_t> out{res.as_span()};
            std::ranges::fill(out.subspan(0U, k), 0xFFFFFFFFFFFFFFFFULL);
            if (k < a.m_size) {
                out[k] -= 1ULL;
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }

        res.resize(a.m_size, 0ULL);
        const std::span<std::uint64_t> out{res.as_span()};
        std::ranges::fill(out.subspan(0U, k), 0xFFFFFFFFFFFFFFFFULL);
        if (k < a.m_size) {
            out[k] = a_span[k] - 1ULL;
            if (a.m_size > k + 1U) {
                BigIntStorage::copy_limbs(out.subspan(k + 1U), a_span.subspan(k + 1U));
            }
        }
        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// Bitwise AND operation: res = a & b with signed two's complement support.
    /// 位元及運算：res = a & b，完整支援二補數負數語意與 SBO 快速路徑。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto bitwise_and(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_sign == 0 || b.m_sign == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (a.m_sign > 0 && b.m_sign > 0) {
            const std::size_t min_len{std::min<std::size_t>(a.m_size, b.m_size)};
            const std::span<const std::uint64_t> a_span{a.as_span()};
            const std::span<const std::uint64_t> b_span{b.as_span()};

            if (&res == &a) {
                const std::span<std::uint64_t> out{res.as_span()};
                if (min_len == 1U) {
                    out[0] &= b_span[0];
                    res.m_size = (out[0] != 0ULL) ? 1U : 0U;
                    res.m_sign = (res.m_size != 0U) ? 1 : 0;
                    return;
                }
                if (min_len == 2U) {
                    out[0] &= b_span[0];
                    out[1] &= b_span[1];
                    res.m_size = (out[1] != 0ULL) ? 2U : ((out[0] != 0ULL) ? 1U : 0U);
                    res.m_sign = (res.m_size != 0U) ? 1 : 0;
                    return;
                }
                if (min_len == 4U) {
                    out[0] &= b_span[0];
                    out[1] &= b_span[1];
                    out[2] &= b_span[2];
                    out[3] &= b_span[3];
                    std::size_t sz{4U};
                    while (sz > 0U && out[sz - 1U] == 0ULL) --sz;
                    res.m_size = static_cast<std::uint32_t>(sz);
                    res.m_sign = (sz != 0U) ? 1 : 0;
                    return;
                }
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] &= b_span[i];
                }
                res.m_size = static_cast<std::uint32_t>(min_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                const std::span<std::uint64_t> out{res.as_span()};
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] &= a_span[i];
                }
                res.m_size = static_cast<std::uint32_t>(min_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(min_len, 0ULL);
            const std::span<std::uint64_t> out{res.as_span()};
            for (std::size_t i{0}; i < min_len; ++i) {
                out[i] = a_span[i] & b_span[i];
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            if (&res == &a || &res == &b) {
                BigIntStorage tmp;
                bitwise_and(tmp, a, b);
                res = std::move(tmp);
                return;
            }
            BigIntStorage not_b;
            bitwise_not(not_b, b);
            res.resize(a.m_size, 0ULL);
            const std::size_t min_len{std::min<std::size_t>(a.m_size, not_b.m_size)};
            const std::span<std::uint64_t> out{res.as_span()};
            const std::span<const std::uint64_t> a_span{a.as_span()};
            const std::span<const std::uint64_t> nb_span{not_b.as_span()};
            for (std::size_t i{0}; i < min_len; ++i) {
                out[i] = a_span[i] & (~nb_span[i]);
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(out.subspan(min_len), a_span.subspan(min_len));
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_and(res, b, a);
            return;
        }
        BigIntStorage not_a;
        BigIntStorage not_b;
        BigIntStorage or_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_or(or_res, not_a, not_b);
        bitwise_not(res, or_res);
    }

    /// <summary>
    /// Bitwise OR operation: res = a | b with two-phase branchless vectorization.
    /// 位元或運算：res = a | b，採重疊區間無分支向量化與長邊區塊拷貝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto bitwise_or(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            const std::size_t min_len{std::min<std::size_t>(a.m_size, b.m_size)};
            const std::size_t max_len{std::max<std::size_t>(a.m_size, b.m_size)};
            const std::span<const std::uint64_t> a_span{a.as_span()};
            const std::span<const std::uint64_t> b_span{b.as_span()};

            if (&res == &a) {
                if (a.m_size == b.m_size) {
                    const std::span<std::uint64_t> out{res.as_span()};
                    if (min_len == 1U) {
                        out[0] |= b_span[0];
                        res.m_size = (out[0] != 0ULL) ? 1U : 0U;
                        res.m_sign = (res.m_size != 0U) ? 1 : 0;
                        return;
                    }
                    if (min_len == 2U) {
                        out[0] |= b_span[0];
                        out[1] |= b_span[1];
                        res.m_size = (out[1] != 0ULL) ? 2U : ((out[0] != 0ULL) ? 1U : 0U);
                        res.m_sign = (res.m_size != 0U) ? 1 : 0;
                        return;
                    }
                    if (min_len == 4U) {
                        out[0] |= b_span[0];
                        out[1] |= b_span[1];
                        out[2] |= b_span[2];
                        out[3] |= b_span[3];
                        res.m_size = 4;
                        res.m_sign = 1;
                        return;
                    }
                }
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                const std::span<std::uint64_t> out{res.capacity_span()};
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] |= b_span[i];
                }
                if (b.m_size > a.m_size) {
                    BigIntStorage::copy_limbs(out.subspan(min_len), b_span.subspan(min_len));
                }
                res.m_size = static_cast<std::uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                const std::span<std::uint64_t> out{res.capacity_span()};
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] |= a_span[i];
                }
                if (a.m_size > b.m_size) {
                    BigIntStorage::copy_limbs(out.subspan(min_len), a_span.subspan(min_len));
                }
                res.m_size = static_cast<std::uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(max_len, 0ULL);
            const std::span<std::uint64_t> out{res.as_span()};
            for (std::size_t i{0}; i < min_len; ++i) {
                out[i] = a_span[i] | b_span[i];
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(out.subspan(min_len), a_span.subspan(min_len));
            } else if (b.m_size > min_len) {
                BigIntStorage::copy_limbs(out.subspan(min_len), b_span.subspan(min_len));
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        BigIntStorage not_a;
        BigIntStorage not_b;
        BigIntStorage and_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_and(and_res, not_a, not_b);
        bitwise_not(res, and_res);
    }

    /// <summary>
    /// Bitwise XOR operation: res = a ^ b with two-phase branchless vectorization.
    /// 位元互斥或運算：res = a ^ b，採重疊區間無分支向量化與長邊區塊拷貝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto bitwise_xor(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            const std::size_t min_len{std::min<std::size_t>(a.m_size, b.m_size)};
            const std::size_t max_len{std::max<std::size_t>(a.m_size, b.m_size)};
            const std::span<const std::uint64_t> a_span{a.as_span()};
            const std::span<const std::uint64_t> b_span{b.as_span()};

            if (&res == &a) {
                if (a.m_size == b.m_size) {
                    const std::span<std::uint64_t> out{res.as_span()};
                    if (min_len == 1U) {
                        out[0] ^= b_span[0];
                        res.m_size = (out[0] != 0ULL) ? 1U : 0U;
                        res.m_sign = (res.m_size != 0U) ? 1 : 0;
                        return;
                    }
                    if (min_len == 2U) {
                        out[0] ^= b_span[0];
                        out[1] ^= b_span[1];
                        res.m_size = (out[1] != 0ULL) ? 2U : ((out[0] != 0ULL) ? 1U : 0U);
                        res.m_sign = (res.m_size != 0U) ? 1 : 0;
                        return;
                    }
                    if (min_len == 4U) {
                        out[0] ^= b_span[0];
                        out[1] ^= b_span[1];
                        out[2] ^= b_span[2];
                        out[3] ^= b_span[3];
                        std::size_t sz{4U};
                        while (sz > 0U && out[sz - 1U] == 0ULL) --sz;
                        res.m_size = static_cast<std::uint32_t>(sz);
                        res.m_sign = (sz != 0U) ? 1 : 0;
                        return;
                    }
                }
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                const std::span<std::uint64_t> out{res.capacity_span()};
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] ^= b_span[i];
                }
                if (b.m_size > a.m_size) {
                    BigIntStorage::copy_limbs(out.subspan(min_len), b_span.subspan(min_len));
                }
                res.m_size = static_cast<std::uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                const std::span<std::uint64_t> out{res.capacity_span()};
                for (std::size_t i{0}; i < min_len; ++i) {
                    out[i] ^= a_span[i];
                }
                if (a.m_size > b.m_size) {
                    BigIntStorage::copy_limbs(out.subspan(min_len), a_span.subspan(min_len));
                }
                res.m_size = static_cast<std::uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(max_len, 0ULL);
            const std::span<std::uint64_t> out{res.as_span()};
            for (std::size_t i{0}; i < min_len; ++i) {
                out[i] = a_span[i] ^ b_span[i];
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(out.subspan(min_len), a_span.subspan(min_len));
            } else if (b.m_size > min_len) {
                BigIntStorage::copy_limbs(out.subspan(min_len), b_span.subspan(min_len));
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            BigIntStorage not_b;
            BigIntStorage xor_res;
            bitwise_not(not_b, b);
            bitwise_xor(xor_res, a, not_b);
            bitwise_not(res, xor_res);
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_xor(res, b, a);
            return;
        }
        BigIntStorage not_a;
        BigIntStorage not_b;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_xor(res, not_a, not_b);
    }
};

} // namespace detail
} // namespace numeric

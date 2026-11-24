// filepath: include/numeric/detail/Arithmetic.hpp
/**
 * @file Arithmetic.hpp
 * @brief Core arbitrary-precision addition, subtraction, and comparison algorithms for CPP-BigInt.
 *        Features 256-bit unrolled SBO fast paths, zero-allocation branch elimination,
 *        std::span bounds safety, and signed dispatch with self-aliasing protection.
 *        核心任意精度加法、減法與比較演算法模組。
 *        具備 256 位元展開 SBO 快速路徑、無分支比較消除、std::span 邊界安全與自我別名防護。
 */
#pragma once

#include "Intrinsics.hpp"
#include "Storage.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// Core unsigned/signed addition, subtraction, and magnitude comparison algorithms.
/// 核心加法、減法與大小比較演算法結構。
/// </summary>
struct BigIntArithmetic : public BigIntIntrinsics {
    /// <summary>
    /// Compares the magnitudes of two unsigned limb spans.
    /// Specialized branchless-friendly fast paths for 1, 2, and 4 limbs.
    /// 比較兩無符號 limbs span 之數值大小，針對 1、2、4 limbs 提供展開快速路徑。
    /// </summary>
    /// <param name="a">First unsigned limb span. / 第一個無符號 limbs span。</param>
    /// <param name="b">Second unsigned limb span. / 第二個無符號 limbs span。</param>
    /// <returns>1 if a > b, -1 if a < b, 0 if equal. / 若 a > b 回傳 1，a < b 回傳 -1，相等回傳 0。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto compare_unsigned(
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b) noexcept -> std::int32_t
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        if (a_len != b_len) {
            return (a_len > b_len) ? 1 : -1;
        }
        if (a_len == 1U) {
            if (a[0] != b[0]) return (a[0] > b[0]) ? 1 : -1;
            return 0;
        }
        if (a_len == 2U) {
            if (a[1] != b[1]) return (a[1] > b[1]) ? 1 : -1;
            if (a[0] != b[0]) return (a[0] > b[0]) ? 1 : -1;
            return 0;
        }
        if (a_len == 4U) {
            if (a[3] != b[3]) return (a[3] > b[3]) ? 1 : -1;
            if (a[2] != b[2]) return (a[2] > b[2]) ? 1 : -1;
            if (a[1] != b[1]) return (a[1] > b[1]) ? 1 : -1;
            if (a[0] != b[0]) return (a[0] > b[0]) ? 1 : -1;
            return 0;
        }
        for (std::size_t i{a_len}; i > 0U; --i) {
            if (a[i - 1U] != b[i - 1U]) {
                return (a[i - 1U] > b[i - 1U]) ? 1 : -1;
            }
        }
        return 0;
    }

    /// <summary>
    /// Unrolled 256-bit SBO fast-path unsigned addition (1..4 limbs).
    /// Pure register ADC pipeline with delayed carry expansion and self-aliasing safety.
    /// 256-bit SBO 靜態展開快路徑加法 (1 ~ 4 Limbs)，純暫存器 ADC 流水線並保證自我別名安全。
    /// </summary>
    /// <param name="res">Destination storage. / 目標輸出儲存體。</param>
    /// <param name="a">First operand storage. / 運算元 a。</param>
    /// <param name="b">Second operand storage. / 運算元 b。</param>
    static NUMERIC_CONSTEXPR_20 auto add_unsigned_sbo4(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        const std::span<const std::uint64_t> a_span{a.as_span()};
        const std::span<const std::uint64_t> b_span{b.as_span()};
        const std::size_t a_size{a_span.size()};
        const std::size_t b_size{b_span.size()};

        const std::uint64_t a0{(0U < a_size) ? a_span[0] : 0ULL};
        const std::uint64_t a1{(1U < a_size) ? a_span[1] : 0ULL};
        const std::uint64_t a2{(2U < a_size) ? a_span[2] : 0ULL};
        const std::uint64_t a3{(3U < a_size) ? a_span[3] : 0ULL};

        const std::uint64_t b0{(0U < b_size) ? b_span[0] : 0ULL};
        const std::uint64_t b1{(1U < b_size) ? b_span[1] : 0ULL};
        const std::uint64_t b2{(2U < b_size) ? b_span[2] : 0ULL};
        const std::uint64_t b3{(3U < b_size) ? b_span[3] : 0ULL};

        std::uint64_t r0{0};
        std::uint64_t r1{0};
        std::uint64_t r2{0};
        std::uint64_t r3{0};
        std::uint8_t c{0};
        c = adc64(c, a0, b0, r0);
        c = adc64(c, a1, b1, r1);
        c = adc64(c, a2, b2, r2);
        c = adc64(c, a3, b3, r3);

        if (c == 0U) {
            std::size_t s{4U};
            if (r3 == 0ULL) {
                if (r2 == 0ULL) {
                    if (r1 == 0ULL) {
                        s = (r0 == 0ULL) ? 0U : 1U;
                    } else {
                        s = 2U;
                    }
                } else {
                    s = 3U;
                }
            }

            if (s <= res.sbo_capacity()) {
                if (!res.is_inline()) {
                    res.reset_heap();
                }
            } else if (res.capacity() < s) {
                res.reserve(s);
            }

            const std::span<std::uint64_t> d{res.capacity_span()};
            switch (s) {
            case 4U: d[3] = r3; [[fallthrough]];
            case 3U: d[2] = r2; [[fallthrough]];
            case 2U: d[1] = r1; [[fallthrough]];
            case 1U: d[0] = r0; break;
            default: break;
            }

            res.m_size = static_cast<std::uint32_t>(s);
            if (s == 0U) {
                res.m_sign = 0;
            }
        } else {
            if (res.capacity() < 5U) {
                res.reserve(5U);
            }
            const std::span<std::uint64_t> d{res.capacity_span()};
            d[0] = r0;
            d[1] = r1;
            d[2] = r2;
            d[3] = r3;
            d[4] = 1ULL;
            res.m_size = 5U;
        }
    }

    /// <summary>
    /// Unrolled 256-bit SBO fast-path unsigned subtraction (1..4 limbs).
    /// Precondition: |a| >= |b| and a.size() <= 4, b.size() <= 4.
    /// 256-bit SBO 靜態展開快路徑減法 (1 ~ 4 Limbs)，前置合約：|a| >= |b| 且皆 <= 4 limbs。
    /// </summary>
    /// <param name="res">Destination storage. / 目標輸出儲存體。</param>
    /// <param name="a">Minuend storage. / 被減數儲存體。</param>
    /// <param name="b">Subtrahend storage. / 減數儲存體。</param>
    static NUMERIC_CONSTEXPR_20 auto sub_unsigned_sbo4(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        const std::span<const std::uint64_t> a_span{a.as_span()};
        const std::span<const std::uint64_t> b_span{b.as_span()};
        const std::size_t a_size{a_span.size()};
        const std::size_t b_size{b_span.size()};

        const std::uint64_t a0{(0U < a_size) ? a_span[0] : 0ULL};
        const std::uint64_t a1{(1U < a_size) ? a_span[1] : 0ULL};
        const std::uint64_t a2{(2U < a_size) ? a_span[2] : 0ULL};
        const std::uint64_t a3{(3U < a_size) ? a_span[3] : 0ULL};

        const std::uint64_t b0{(0U < b_size) ? b_span[0] : 0ULL};
        const std::uint64_t b1{(1U < b_size) ? b_span[1] : 0ULL};
        const std::uint64_t b2{(2U < b_size) ? b_span[2] : 0ULL};
        const std::uint64_t b3{(3U < b_size) ? b_span[3] : 0ULL};

        std::uint64_t r0{0};
        std::uint64_t r1{0};
        std::uint64_t r2{0};
        std::uint64_t r3{0};
        std::uint8_t borrow{0};
        borrow = sbb64(borrow, a0, b0, r0);
        borrow = sbb64(borrow, a1, b1, r1);
        borrow = sbb64(borrow, a2, b2, r2);
        (void)sbb64(borrow, a3, b3, r3);

        std::size_t s{4U};
        if (r3 == 0ULL) {
            if (r2 == 0ULL) {
                if (r1 == 0ULL) {
                    s = (r0 == 0ULL) ? 0U : 1U;
                } else {
                    s = 2U;
                }
            } else {
                s = 3U;
            }
        }

        if (s <= res.sbo_capacity()) {
            if (!res.is_inline()) {
                res.reset_heap();
            }
        } else if (res.capacity() < s) {
            res.reserve(s);
        }

        const std::span<std::uint64_t> d{res.capacity_span()};
        switch (s) {
        case 4U: d[3] = r3; [[fallthrough]];
        case 3U: d[2] = r2; [[fallthrough]];
        case 2U: d[1] = r1; [[fallthrough]];
        case 1U: d[0] = r0; break;
        default: break;
        }

        res.m_size = static_cast<std::uint32_t>(s);
        if (s == 0U) {
            res.m_sign = 0;
        }
    }

    /// <summary>
    /// General multi-limb unsigned addition with alias backup, pre-reservation, and early-exit carry loop.
    /// 多肢段通用加法，含別名備份防護、空間預留與長邊 Early-Exit 進位鏈。
    /// </summary>
    /// <param name="res">Destination storage. / 目標輸出儲存體。</param>
    /// <param name="a">First operand storage. / 運算元 a。</param>
    /// <param name="b">Second operand storage. / 運算元 b。</param>
    static NUMERIC_CONSTEXPR_20 auto add_unsigned_general(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        const std::size_t a_len{a.m_size};
        const std::size_t b_len{b.m_size};
        const std::size_t min_len{std::min(a_len, b_len)};
        const std::size_t max_len{std::max(a_len, b_len)};

        std::span<const std::uint64_t> a_span{a.as_span()};
        std::span<const std::uint64_t> b_span{b.as_span()};

        // Alias backup is only needed when res aliases the shorter operand and may reallocate
        // 僅於 res 別名較短運算元並可能重新配置時才需備份（避開常規熱路徑之堆疊歸零與 EH 開銷）
        if ((&res == &a && a_len < b_len) || (&res == &b && b_len < a_len)) {
            if (std::is_constant_evaluated()) {
                std::array<std::uint64_t, 128> ce_stack{};
                std::vector<std::uint64_t> ce_heap{};
                if (&res == &a) {
                    if (a_len <= 128U) {
                        std::ranges::copy(a.as_span(), ce_stack.begin());
                        a_span = std::span<const std::uint64_t>{ce_stack.data(), a_len};
                    } else {
                        ce_heap.assign(a.as_span().begin(), a.as_span().end());
                        a_span = std::span<const std::uint64_t>{ce_heap};
                    }
                    res.resize(max_len + 1U, 0ULL);
                } else {
                    if (b_len <= 128U) {
                        std::ranges::copy(b.as_span(), ce_stack.begin());
                        b_span = std::span<const std::uint64_t>{ce_stack.data(), b_len};
                    } else {
                        ce_heap.assign(b.as_span().begin(), b.as_span().end());
                        b_span = std::span<const std::uint64_t>{ce_heap};
                    }
                    res.resize(max_len + 1U, 0ULL);
                }
                const std::span<const std::uint64_t> ce_longer{(a_len >= b_len) ? a_span : b_span};
                const std::span<std::uint64_t> ce_out{res.capacity_span()};
                std::uint8_t c{0};
                for (std::size_t i{0}; i < min_len; ++i) {
                    c = adc64(c, a_span[i], b_span[i], ce_out[i]);
                }
                std::size_t idx{min_len};
                for (; idx < max_len && c != 0U; ++idx) {
                    c = adc64(c, ce_longer[idx], 0ULL, ce_out[idx]);
                }
                if (c == 0U) {
                    if (res.data() != ce_longer.data() && idx < max_len) {
                        BigIntStorage::copy_limbs(ce_out.subspan(idx), ce_longer.subspan(idx, max_len - idx));
                    }
                    res.m_size = static_cast<std::uint32_t>(max_len);
                } else {
                    if (res.m_size < max_len + 1U) {
                        res.resize(max_len + 1U, 0ULL);
                    }
                    res[max_len] = 1ULL;
                    res.m_size = static_cast<std::uint32_t>(max_len + 1U);
                }
                return;
            }

            if (&res == &a) {
                if (a_len <= 128U) {
                    std::array<std::uint64_t, 128> a_stack;
                    std::ranges::copy(a.as_span(), a_stack.begin());
                    res.resize(max_len + 1U, 0ULL);
                    const std::span<std::uint64_t> out_span{res.capacity_span()};
                    std::uint8_t carry{0};
                    for (std::size_t i{0}; i < min_len; ++i) {
                        carry = adc64(carry, a_stack[i], b_span[i], out_span[i]);
                    }
                    std::size_t i{min_len};
                    for (; i < max_len && carry != 0U; ++i) {
                        carry = adc64(carry, b_span[i], 0ULL, out_span[i]);
                    }
                    if (carry == 0U) {
                        if (i < max_len) {
                            BigIntStorage::copy_limbs(out_span.subspan(i), b_span.subspan(i, max_len - i));
                        }
                        res.m_size = static_cast<std::uint32_t>(max_len);
                    } else {
                        res[max_len] = 1ULL;
                        res.m_size = static_cast<std::uint32_t>(max_len + 1U);
                    }
                    return;
                }
                std::vector<std::uint64_t> a_heap(a.as_span().begin(), a.as_span().end());
                res.resize(max_len + 1U, 0ULL);
                const std::span<std::uint64_t> out_span{res.capacity_span()};
                std::uint8_t carry{0};
                for (std::size_t i{0}; i < min_len; ++i) {
                    carry = adc64(carry, a_heap[i], b_span[i], out_span[i]);
                }
                std::size_t i{min_len};
                for (; i < max_len && carry != 0U; ++i) {
                    carry = adc64(carry, b_span[i], 0ULL, out_span[i]);
                }
                if (carry == 0U) {
                    if (i < max_len) {
                        BigIntStorage::copy_limbs(out_span.subspan(i), b_span.subspan(i, max_len - i));
                    }
                    res.m_size = static_cast<std::uint32_t>(max_len);
                } else {
                    res[max_len] = 1ULL;
                    res.m_size = static_cast<std::uint32_t>(max_len + 1U);
                }
                return;
            }

            // &res == &b && b_len < a_len
            if (b_len <= 128U) {
                std::array<std::uint64_t, 128> b_stack;
                std::ranges::copy(b.as_span(), b_stack.begin());
                res.resize(max_len + 1U, 0ULL);
                const std::span<std::uint64_t> out_span{res.capacity_span()};
                std::uint8_t carry{0};
                for (std::size_t i{0}; i < min_len; ++i) {
                    carry = adc64(carry, a_span[i], b_stack[i], out_span[i]);
                }
                std::size_t i{min_len};
                for (; i < max_len && carry != 0U; ++i) {
                    carry = adc64(carry, a_span[i], 0ULL, out_span[i]);
                }
                if (carry == 0U) {
                    if (i < max_len) {
                        BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, max_len - i));
                    }
                    res.m_size = static_cast<std::uint32_t>(max_len);
                } else {
                    res[max_len] = 1ULL;
                    res.m_size = static_cast<std::uint32_t>(max_len + 1U);
                }
                return;
            }
            std::vector<std::uint64_t> b_heap(b.as_span().begin(), b.as_span().end());
            res.resize(max_len + 1U, 0ULL);
            const std::span<std::uint64_t> out_span{res.capacity_span()};
            std::uint8_t carry{0};
            for (std::size_t i{0}; i < min_len; ++i) {
                carry = adc64(carry, a_span[i], b_heap[i], out_span[i]);
            }
            std::size_t i{min_len};
            for (; i < max_len && carry != 0U; ++i) {
                carry = adc64(carry, a_span[i], 0ULL, out_span[i]);
            }
            if (carry == 0U) {
                if (i < max_len) {
                    BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, max_len - i));
                }
                res.m_size = static_cast<std::uint32_t>(max_len);
            } else {
                res[max_len] = 1ULL;
                res.m_size = static_cast<std::uint32_t>(max_len + 1U);
            }
            return;
        }

        if (&res != &a && &res != &b) {
            res.resize(max_len + 1U, 0ULL);
        }

        const std::span<const std::uint64_t> longer_span{(a_len >= b_len) ? a_span : b_span};
        const std::span<std::uint64_t> out_span{res.capacity_span()};

        std::uint8_t carry{0};
        for (std::size_t i{0}; i < min_len; ++i) {
            carry = adc64(carry, a_span[i], b_span[i], out_span[i]);
        }

        std::size_t i{min_len};
        for (; i < max_len && carry != 0U; ++i) {
            carry = adc64(carry, longer_span[i], 0ULL, out_span[i]);
        }

        if (carry == 0U) {
            if (res.data() != longer_span.data() && i < max_len) {
                BigIntStorage::copy_limbs(
                    out_span.subspan(i),
                    longer_span.subspan(i, max_len - i));
            }
            res.m_size = static_cast<std::uint32_t>(max_len);
        } else {
            if (res.m_size < max_len + 1U) {
                res.resize(max_len + 1U, 0ULL);
            }
            res[max_len] = 1ULL;
            res.m_size = static_cast<std::uint32_t>(max_len + 1U);
        }
    }

    /// <summary>
    /// Unsigned limb addition: res = a + b (magnitude only).
    /// 無符號 limbs 加法：res = a + b（純數值大小運算）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto add_unsigned(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        if (a.m_size <= 4U && b.m_size <= 4U) {
            add_unsigned_sbo4(res, a, b);
        } else {
            add_unsigned_general(res, a, b);
        }
    }

    /// <summary>
    /// Core magnitude subtraction: res = a - b (Precondition: |a| >= |b|).
    /// Supports alias backup (&res == &b), pre-allocation, and fast top-down normalization.
    /// 絕對值減法核心演算法：res = a - b（前置合約：|a| >= |b|），支援別名安全與快速規格化。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto sub_magnitude_core(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        const std::size_t a_len{a.m_size};
        const std::size_t b_len{b.m_size};

        if (a_len <= 4U && b_len <= 4U) {
            sub_unsigned_sbo4(res, a, b);
            return;
        }

        if (&res == &b) {
            if (std::is_constant_evaluated()) {
                std::array<std::uint64_t, 128> b_stack{};
                std::vector<std::uint64_t> b_heap{};
                std::span<const std::uint64_t> b_span{b.as_span()};
                if (b_len <= 128U) {
                    std::ranges::copy(b.as_span(), b_stack.begin());
                    b_span = std::span<const std::uint64_t>{b_stack.data(), b_len};
                } else {
                    b_heap.assign(b.as_span().begin(), b.as_span().end());
                    b_span = std::span<const std::uint64_t>{b_heap};
                }
                const std::span<const std::uint64_t> a_span{a.as_span()};
                const std::span<std::uint64_t> out_span{res.as_span()};
                std::uint8_t borrow{0};
                for (std::size_t i{0}; i < b_len; ++i) {
                    borrow = sbb64(borrow, a_span[i], b_span[i], out_span[i]);
                }
                std::size_t i{b_len};
                for (; i < a_len && borrow != 0U; ++i) {
                    borrow = sbb64(borrow, a_span[i], 0ULL, out_span[i]);
                }
                if (borrow == 0U && i < a_len) {
                    BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, a_len - i));
                }
                res.m_size = static_cast<std::uint32_t>(a_len);
                return;
            }
            if (b_len <= 128U) {
                std::array<std::uint64_t, 128> b_stack;
                std::ranges::copy(b.as_span(), b_stack.begin());
                const std::span<const std::uint64_t> a_span{a.as_span()};
                const std::span<std::uint64_t> out_span{res.as_span()};
                std::uint8_t borrow{0};
                for (std::size_t i{0}; i < b_len; ++i) {
                    borrow = sbb64(borrow, a_span[i], b_stack[i], out_span[i]);
                }
                std::size_t i{b_len};
                for (; i < a_len && borrow != 0U; ++i) {
                    borrow = sbb64(borrow, a_span[i], 0ULL, out_span[i]);
                }
                if (borrow == 0U && i < a_len) {
                    BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, a_len - i));
                }
                res.m_size = static_cast<std::uint32_t>(a_len);
                return;
            }
            std::vector<std::uint64_t> b_heap(b.as_span().begin(), b.as_span().end());
            const std::span<const std::uint64_t> a_span{a.as_span()};
            const std::span<std::uint64_t> out_span{res.as_span()};
            std::uint8_t borrow{0};
            for (std::size_t i{0}; i < b_len; ++i) {
                borrow = sbb64(borrow, a_span[i], b_heap[i], out_span[i]);
            }
            std::size_t i{b_len};
            for (; i < a_len && borrow != 0U; ++i) {
                borrow = sbb64(borrow, a_span[i], 0ULL, out_span[i]);
            }
            if (borrow == 0U && i < a_len) {
                BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, a_len - i));
            }
            res.m_size = static_cast<std::uint32_t>(a_len);
            return;
        }

        if (&res != &a) {
            res.resize(a_len, 0ULL);
        }

        const std::span<const std::uint64_t> b_span{b.as_span()};
        const std::span<const std::uint64_t> a_span{a.as_span()};
        const std::span<std::uint64_t> out_span{res.as_span()};

        std::uint8_t borrow{0};
        for (std::size_t i{0}; i < b_len; ++i) {
            borrow = sbb64(borrow, a_span[i], b_span[i], out_span[i]);
        }
        std::size_t i{b_len};
        for (; i < a_len && borrow != 0U; ++i) {
            borrow = sbb64(borrow, a_span[i], 0ULL, out_span[i]);
        }
        if (borrow == 0U && &res != &a && i < a_len) {
            BigIntStorage::copy_limbs(out_span.subspan(i), a_span.subspan(i, a_len - i));
        }

        std::size_t sz{a_len};
        while (sz > 0U && out_span[sz - 1U] == 0ULL) {
            --sz;
        }
        res.m_size = static_cast<std::uint32_t>(sz);
        if (sz == 0U) {
            res.m_sign = 0;
        }
        if (sz <= res.m_sbo_capacity) {
            res.shrink_to_sbo_if_possible();
        }
    }

    /// <summary>
    /// Unsigned limb subtraction: res = a - b (Precondition: a >= b).
    /// 無符號 limbs 減法：res = a - b（前置條件：a >= b）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto sub_unsigned(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) noexcept -> void
    {
        if (a.m_size <= 4U && b.m_size <= 4U) {
            sub_unsigned_sbo4(res, a, b);
        } else {
            sub_magnitude_core(res, a, b);
        }
        res.m_sign = (res.m_size > 0U) ? 1 : 0;
    }

    /// <summary>
    /// Signed integer addition: res = a + b.
    /// 帶符號整數加法：res = a + b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto add_signed(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_sign == 0) {
            res = b;
            return;
        }
        if (b.m_sign == 0) {
            res = a;
            return;
        }
        if (a.m_sign == b.m_sign) {
            add_unsigned(res, a, b);
            res.m_sign = (res.m_size > 0U) ? a.m_sign : 0;
        } else {
            const std::int32_t cmp{compare_unsigned(a.as_span(), b.as_span())};
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                if (a.m_size <= 4U && b.m_size <= 4U) {
                    sub_unsigned_sbo4(res, a, b);
                } else {
                    sub_magnitude_core(res, a, b);
                }
                res.m_sign = (res.m_size > 0U) ? a.m_sign : 0;
            } else {
                if (a.m_size <= 4U && b.m_size <= 4U) {
                    sub_unsigned_sbo4(res, b, a);
                } else {
                    sub_magnitude_core(res, b, a);
                }
                res.m_sign = (res.m_size > 0U) ? b.m_sign : 0;
            }
        }
    }

    /// <summary>
    /// Signed integer subtraction: res = a - b.
    /// 帶符號整數減法：res = a - b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto sub_signed(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (b.m_sign == 0) {
            res = a;
            return;
        }
        if (a.m_sign == 0) {
            res = b;
            res.m_sign = static_cast<std::int8_t>(-res.m_sign);
            return;
        }
        if (a.m_sign != b.m_sign) {
            add_unsigned(res, a, b);
            res.m_sign = (res.m_size > 0U) ? a.m_sign : 0;
        } else {
            const std::int32_t cmp{compare_unsigned(a.as_span(), b.as_span())};
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                if (a.m_size <= 4U && b.m_size <= 4U) {
                    sub_unsigned_sbo4(res, a, b);
                } else {
                    sub_magnitude_core(res, a, b);
                }
                res.m_sign = (res.m_size > 0U) ? a.m_sign : 0;
            } else {
                if (a.m_size <= 4U && b.m_size <= 4U) {
                    sub_unsigned_sbo4(res, b, a);
                } else {
                    sub_magnitude_core(res, b, a);
                }
                res.m_sign = (res.m_size > 0U) ? static_cast<std::int8_t>(-b.m_sign) : 0;
            }
        }
    }
};

} // namespace detail
} // namespace numeric

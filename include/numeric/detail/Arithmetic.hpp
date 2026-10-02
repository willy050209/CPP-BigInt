#pragma once

// Arithmetic.hpp
// Core addition, subtraction, and comparison algorithms for CPP-BigInt.
// Features 256-bit unrolled SBO fast-paths, zero-allocation branch elimination, and signed dispatch.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "Intrinsics.hpp"
#include "Storage.hpp"
#include <algorithm>
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// 核心加法與減法演算法結構。
/// </summary>
struct BigIntArithmetic : public BigIntIntrinsics {
    /// <summary>
    /// 比較兩無符號 limbs 陣列之大小。
    /// </summary>
    /// <param name="a">陣列 a</param>
    /// <param name="a_len">陣列 a 長度</param>
    /// <param name="b">陣列 b</param>
    /// <param name="b_len">陣列 b 長度</param>
    /// <returns>若 a &gt; b 回傳 1，a &lt; b 回傳 -1，相等回傳 0</returns>
    static NUMERIC_CONSTEXPR_20 int compare_unsigned(const uint64_t* a, size_t a_len, const uint64_t* b, size_t b_len) noexcept {
        if (a_len > b_len) return 1;
        if (a_len < b_len) return -1;
        for (size_t i = a_len; i > 0; --i) {
            if (a[i - 1] > b[i - 1]) return 1;
            if (a[i - 1] < b[i - 1]) return -1;
        }
        return 0;
    }

    /// <summary>
    /// 256-bit SBO 靜態展開快路徑加法 (1 ~ 4 Limbs)。
    /// 安全零填充提取，純暫存器 ADC 流水線，延遲進位擴容，保證自我別名與全零規格化安全。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned_sbo4(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        const uint64_t* a_data = a.data();
        const uint64_t* b_data = b.data();
        size_t a_size = a.m_size;
        size_t b_size = b.m_size;

        const uint64_t a0 = (0 < a_size) ? a_data[0] : 0;
        const uint64_t a1 = (1 < a_size) ? a_data[1] : 0;
        const uint64_t a2 = (2 < a_size) ? a_data[2] : 0;
        const uint64_t a3 = (3 < a_size) ? a_data[3] : 0;

        const uint64_t b0 = (0 < b_size) ? b_data[0] : 0;
        const uint64_t b1 = (1 < b_size) ? b_data[1] : 0;
        const uint64_t b2 = (2 < b_size) ? b_data[2] : 0;
        const uint64_t b3 = (3 < b_size) ? b_data[3] : 0;

        uint64_t r0 = 0, r1 = 0, r2 = 0, r3 = 0;
        uint8_t c = 0;
        c = adc64(c, a0, b0, &r0);
        c = adc64(c, a1, b1, &r1);
        c = adc64(c, a2, b2, &r2);
        c = adc64(c, a3, b3, &r3);

        if (c == 0) {
            size_t s = 4;
            if (r3 == 0) {
                if (r2 == 0) {
                    if (r1 == 0) s = (r0 == 0) ? 0 : 1;
                    else s = 2;
                } else {
                    s = 3;
                }
            }

            if (s <= res.sbo_capacity()) {
                if (!res.is_inline()) {
                    res.reset_heap();
                }
            } else if (res.capacity() < s) {
                res.reserve(s);
            }

            uint64_t* d = res.data();
            switch (s) {
            case 4: d[3] = r3; /* fallthrough */
            case 3: d[2] = r2; /* fallthrough */
            case 2: d[1] = r1; /* fallthrough */
            case 1: d[0] = r0; break;
            default: break;
            }

            res.m_size = static_cast<uint32_t>(s);
            if (s == 0) {
                res.m_sign = 0;
            }
        } else {
            if (res.capacity() < 5) {
                res.reserve(5);
            }
            uint64_t* d = res.data();
            d[0] = r0; d[1] = r1; d[2] = r2; d[3] = r3;
            d[4] = 1;
            res.m_size = 5;
        }
    }

    /// <summary>
    /// 256-bit SBO 靜態展開快路徑減法 (1 ~ 4 Limbs)。
    /// 前置條件：|a| >= |b| 且 a.m_size <= 4, b.m_size <= 4。
    /// 純暫存器 SBB 流水線，直接輸出至 SBO 陣列，保證自我別名與全零規格化安全。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_unsigned_sbo4(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        const uint64_t* a_data = a.data();
        const uint64_t* b_data = b.data();
        size_t a_size = a.m_size;
        size_t b_size = b.m_size;

        const uint64_t a0 = (0 < a_size) ? a_data[0] : 0;
        const uint64_t a1 = (1 < a_size) ? a_data[1] : 0;
        const uint64_t a2 = (2 < a_size) ? a_data[2] : 0;
        const uint64_t a3 = (3 < a_size) ? a_data[3] : 0;

        const uint64_t b0 = (0 < b_size) ? b_data[0] : 0;
        const uint64_t b1 = (1 < b_size) ? b_data[1] : 0;
        const uint64_t b2 = (2 < b_size) ? b_data[2] : 0;
        const uint64_t b3 = (3 < b_size) ? b_data[3] : 0;

        uint64_t r0 = 0, r1 = 0, r2 = 0, r3 = 0;
        uint8_t borrow = 0;
        borrow = sbb64(borrow, a0, b0, &r0);
        borrow = sbb64(borrow, a1, b1, &r1);
        borrow = sbb64(borrow, a2, b2, &r2);
        borrow = sbb64(borrow, a3, b3, &r3);

        size_t s = 4;
        if (r3 == 0) {
            if (r2 == 0) {
                if (r1 == 0) s = (r0 == 0) ? 0 : 1;
                else s = 2;
            } else {
                s = 3;
            }
        }

        if (s <= res.sbo_capacity()) {
            if (!res.is_inline()) {
                res.reset_heap();
            }
        } else if (res.capacity() < s) {
            res.reserve(s);
        }

        uint64_t* d = res.data();
        switch (s) {
        case 4: d[3] = r3; /* fallthrough */
        case 3: d[2] = r2; /* fallthrough */
        case 2: d[1] = r1; /* fallthrough */
        case 1: d[0] = r0; break;
        default: break;
        }

        res.m_size = static_cast<uint32_t>(s);
        if (s == 0) {
            res.m_sign = 0;
        }
    }

    /// <summary>
    /// 多肢段通用加法，含別名指針備份防護、空間預留與長邊 Early-Exit 進位鏈。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned_general(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        size_t a_len = a.m_size;
        size_t b_len = b.m_size;
        size_t min_len = std::min(a_len, b_len);
        size_t max_len = std::max(a_len, b_len);

        uint64_t a_stack[128], b_stack[128];
        const uint64_t* a_ptr = a.data();
        const uint64_t* b_ptr = b.data();
        std::vector<uint64_t> a_heap, b_heap;

        if (&res == &a && a_len < b_len) {
            if (a_len <= 128) {
                std::copy_n(a.data(), a_len, a_stack);
                a_ptr = a_stack;
            } else {
                a_heap.resize(a_len);
                std::copy_n(a.data(), a_len, a_heap.data());
                a_ptr = a_heap.data();
            }
        } else if (&res == &b && b_len < a_len) {
            if (b_len <= 128) {
                std::copy_n(b.data(), b_len, b_stack);
                b_ptr = b_stack;
            } else {
                b_heap.resize(b_len);
                std::copy_n(b.data(), b_len, b_heap.data());
                b_ptr = b_heap.data();
            }
        }

        if (&res != &a && &res != &b) {
            res.resize(max_len + 1, 0);
        } else if ((&res == &a && a_len < b_len) || (&res == &b && b_len < a_len)) {
            res.resize(max_len + 1, 0);
        }

        const uint64_t* longer_ptr = (a_len >= b_len) ? a_ptr : b_ptr;

        uint8_t carry = 0;
        for (size_t i = 0; i < min_len; ++i) {
            carry = adc64(carry, a_ptr[i], b_ptr[i], &res.data()[i]);
        }

        size_t i = min_len;
        for (; i < max_len && carry != 0; ++i) {
            carry = adc64(carry, longer_ptr[i], 0, &res.data()[i]);
        }

        if (carry == 0) {
            if (res.data() != longer_ptr && i < max_len) {
                std::copy_n(longer_ptr + i, max_len - i, res.data() + i);
            }
            res.m_size = static_cast<uint32_t>(max_len);
        } else {
            if (res.m_size < max_len + 1) {
                res.resize(max_len + 1, 0);
            }
            res.data()[max_len] = 1;
            res.m_size = static_cast<uint32_t>(max_len + 1);
        }
    }

    /// <summary>
    /// 無符號 limbs 加法：res = a + b。純數值 (Magnitude) 運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        if (a.m_size <= 4 && b.m_size <= 4) {
            add_unsigned_sbo4(res, a, b);
        } else {
            add_unsigned_general(res, a, b);
        }
    }

    /// <summary>
    /// 絕對值減法核心演算法：res = a - b (前置合約：|a| >= |b|)。
    /// 支援小運算元別名備份 (&res == &b)、前置擴容與由高位向下快速規格化。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_magnitude_core(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        size_t a_len = a.m_size;
        size_t b_len = b.m_size;

        if (a_len <= 4 && b_len <= 4) {
            sub_unsigned_sbo4(res, a, b);
            return;
        }

        uint64_t b_stack[128];
        const uint64_t* b_data = b.data();
        std::vector<uint64_t> b_heap;
        if (&res == &b) {
            if (b_len <= 128) {
                std::copy_n(b.data(), b_len, b_stack);
                b_data = b_stack;
            } else {
                b_heap.resize(b_len);
                std::copy_n(b.data(), b_len, b_heap.data());
                b_data = b_heap.data();
            }
        }

        if (&res != &a) {
            res.resize(a_len, 0);
        }

        uint8_t borrow = 0;
        for (size_t i = 0; i < b_len; ++i) {
            borrow = sbb64(borrow, a.data()[i], b_data[i], &res.data()[i]);
        }
        size_t i = b_len;
        for (; i < a_len && borrow != 0; ++i) {
            borrow = sbb64(borrow, a.data()[i], 0, &res.data()[i]);
        }
        if (borrow == 0 && &res != &a && i < a_len) {
            std::copy_n(a.data() + i, a_len - i, res.data() + i);
        }

        size_t sz = a_len;
        while (sz > 0 && res.data()[sz - 1] == 0) --sz;
        res.m_size = static_cast<uint32_t>(sz);
        if (sz == 0) {
            res.m_sign = 0;
        }
        if (sz <= res.m_sbo_capacity) {
            res.shrink_to_sbo_if_possible();
        }
    }

    /// <summary>
    /// 無符號 limbs 減法：res = a - b（前置條件：a >= b）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_unsigned(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept {
        if (a.m_size <= 4 && b.m_size <= 4) {
            sub_unsigned_sbo4(res, a, b);
        } else {
            sub_magnitude_core(res, a, b);
        }
        res.m_sign = (res.m_size > 0) ? 1 : 0;
    }

    /// <summary>
    /// 帶符號加法：res = a + b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
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
            res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        } else {
            int cmp = compare_unsigned(a.data(), a.m_size, b.data(), b.m_size);
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                if (a.m_size <= 4 && b.m_size <= 4) {
                    sub_unsigned_sbo4(res, a, b);
                } else {
                    sub_magnitude_core(res, a, b);
                }
                res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
            } else {
                if (a.m_size <= 4 && b.m_size <= 4) {
                    sub_unsigned_sbo4(res, b, a);
                } else {
                    sub_magnitude_core(res, b, a);
                }
                res.m_sign = (res.m_size > 0) ? b.m_sign : 0;
            }
        }
    }

    /// <summary>
    /// 帶符號減法：res = a - b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (b.m_sign == 0) {
            res = a;
            return;
        }
        if (a.m_sign == 0) {
            res = b;
            res.m_sign = -res.m_sign;
            return;
        }
        if (a.m_sign != b.m_sign) {
            add_unsigned(res, a, b);
            res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        } else {
            int cmp = compare_unsigned(a.data(), a.m_size, b.data(), b.m_size);
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                if (a.m_size <= 4 && b.m_size <= 4) {
                    sub_unsigned_sbo4(res, a, b);
                } else {
                    sub_magnitude_core(res, a, b);
                }
                res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
            } else {
                if (a.m_size <= 4 && b.m_size <= 4) {
                    sub_unsigned_sbo4(res, b, a);
                } else {
                    sub_magnitude_core(res, b, a);
                }
                res.m_sign = (res.m_size > 0) ? -b.m_sign : 0;
            }
        }
    }
};

} // namespace detail
} // namespace numeric

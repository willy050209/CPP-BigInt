#pragma once

// Bitwise.hpp
// Arbitrary-precision bitwise logic and shifting operations for CPP-BigInt.
// Part of the numeric::detail modular core.

#include "Arithmetic.hpp"

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 位元邏輯與位移演算法類別，繼承自 BigIntArithmetic。
/// 支援左移、算術/邏輯右移、NOT、AND、OR、XOR 等完整位元運算。
/// </summary>
struct BigIntBitwise : public BigIntArithmetic {
    /// <summary>
    /// limbs 級別的高效整區塊左移：res = a << (limbs * 64)。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">輸入數值</param>
    /// <param name="limbs">移動 limbs 數量</param>
    static NUMERIC_CONSTEXPR_20 void shift_left_limbs(BigIntStorage& res, const BigIntStorage& a, size_t limbs) {
        if (a.m_size == 0) {
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
        res.resize(a.m_size + limbs, 0);
        BigIntStorage::copy_limbs(res.data() + limbs, a.data(), a.m_size);
        BigIntStorage::zero_limbs(res.data(), limbs);
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// 位元左移運算：res = a << shift。
    /// 透過 shld64 硬體指令消除迴圈內分支與多餘運算。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_left(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (shift == 0 || a.m_size == 0) {
            res = a;
            return;
        }
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        bool overflow = (bit_shift != 0) && ((a.data()[a.m_size - 1] >> (64 - bit_shift)) != 0);
        size_t new_size = a.m_size + limb_shift + (overflow ? 1 : 0);

        if (&res == &a) {
            const size_t old_size = a.m_size;
            if (res.m_capacity < new_size) {
                res.reserve(new_size);
            }
            res.m_size = static_cast<uint32_t>(new_size);
            uint64_t* d = res.data();
            if (limb_shift == 0) {
                if (old_size == 1) {
                    uint64_t v0 = d[0];
                    if (overflow) d[1] = v0 >> (64 - bit_shift);
                    d[0] = v0 << bit_shift;
                    return;
                }
                if (old_size == 2) {
                    uint64_t v0 = d[0], v1 = d[1];
                    if (overflow) d[2] = v1 >> (64 - bit_shift);
                    d[1] = shld64(v1, v0, static_cast<unsigned int>(bit_shift));
                    d[0] = v0 << bit_shift;
                    return;
                }
                if (old_size == 4) {
                    uint64_t v0 = d[0], v1 = d[1], v2 = d[2], v3 = d[3];
                    if (overflow) d[4] = v3 >> (64 - bit_shift);
                    d[3] = shld64(v3, v2, static_cast<unsigned int>(bit_shift));
                    d[2] = shld64(v2, v1, static_cast<unsigned int>(bit_shift));
                    d[1] = shld64(v1, v0, static_cast<unsigned int>(bit_shift));
                    d[0] = v0 << bit_shift;
                    return;
                }
                const unsigned int bshift = static_cast<unsigned int>(bit_shift);
                if (overflow) {
                    d[old_size] = d[old_size - 1] >> (64 - bit_shift);
                }
                for (size_t i = old_size - 1; i > 0; --i) {
                    d[i] = shld64(d[i], d[i - 1], bshift);
                }
                d[0] <<= bshift;
                return;
            }
            if (bit_shift == 0) {
                for (size_t i = old_size; i > 0; --i) {
                    d[i - 1 + limb_shift] = d[i - 1];
                }
                if (limb_shift > 0) {
                    BigIntStorage::zero_limbs(d, limb_shift);
                }
            } else {
                const unsigned int bshift = static_cast<unsigned int>(bit_shift);
                if (overflow) {
                    d[old_size + limb_shift] = d[old_size - 1] >> (64 - bit_shift);
                }
                for (size_t i = old_size - 1; i > 0; --i) {
                    d[i + limb_shift] = shld64(d[i], d[i - 1], bshift);
                }
                d[limb_shift] = d[0] << bshift;
                if (limb_shift > 0) {
                    BigIntStorage::zero_limbs(d, limb_shift);
                }
            }
            return;
        }

        res.resize(new_size, 0);

        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.data() + limb_shift, a.data(), a.m_size);
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.data(), limb_shift);
            }
        } else {
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.data(), limb_shift);
            }
            uint64_t prev = 0;
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t cur = a.data()[i];
                res.data()[i + limb_shift] = shld64(cur, prev, static_cast<unsigned int>(bit_shift));
                prev = cur;
            }
            if (overflow) {
                res.data()[a.m_size + limb_shift] = prev >> (64 - bit_shift);
            }
        }
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// 位元右移運算：res = a >> shift（支援負數算術右移語意）。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_right(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (shift == 0 || a.m_size == 0) {
            res = a;
            return;
        }
        if (a.m_sign < 0) {
            // 負數算術右移：a >> shift = ~((~a) >> shift)
            BigIntStorage u, shifted_u;
            bitwise_not(u, a);
            shift_right_positive(shifted_u, u, shift);
            bitwise_not(res, shifted_u);
            return;
        }
        shift_right_positive(res, a, shift);
    }

    /// <summary>
    /// 正整數無符號位元右移運算。
    /// In-place unsigned bitwise right shift operation.
    /// 透過 shrd64 硬體指令與原地前向搬移消除迴圈內分支與暫存物件配置。
    /// </summary>
    /// <param name="res">輸出結果 / Output result</param>
    /// <param name="a">運算元 / Operand a</param>
    /// <param name="shift">位移位元數 / Bit shift count</param>
    static NUMERIC_CONSTEXPR_20 void shift_right_positive(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        const size_t old_size = a.m_size;
        if (limb_shift >= old_size) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        size_t new_size = old_size - limb_shift;

        if (&res == &a) {
            uint64_t* d = res.data();
            if (limb_shift == 0) {
                if (old_size == 1) {
                    d[0] >>= bit_shift;
                    res.m_size = (d[0] != 0) ? 1 : 0;
                    if (res.m_size == 0) res.m_sign = 0;
                    return;
                }
                if (old_size == 2) {
                    uint64_t v0 = d[0], v1 = d[1];
                    d[0] = shrd64(v1, v0, static_cast<unsigned int>(bit_shift));
                    uint64_t top = v1 >> bit_shift;
                    d[1] = top;
                    res.m_size = (top != 0) ? 2 : ((d[0] != 0) ? 1 : 0);
                    if (res.m_size == 0) res.m_sign = 0;
                    return;
                }
                if (old_size == 4) {
                    uint64_t v0 = d[0], v1 = d[1], v2 = d[2], v3 = d[3];
                    d[0] = shrd64(v1, v0, static_cast<unsigned int>(bit_shift));
                    d[1] = shrd64(v2, v1, static_cast<unsigned int>(bit_shift));
                    d[2] = shrd64(v3, v2, static_cast<unsigned int>(bit_shift));
                    uint64_t top = v3 >> bit_shift;
                    d[3] = top;
                    size_t sz = 4;
                    while (sz > 0 && d[sz - 1] == 0) --sz;
                    res.m_size = static_cast<uint32_t>(sz);
                    if (sz == 0) res.m_sign = 0;
                    return;
                }
            }
            if (bit_shift == 0) {
                if (limb_shift > 0) {
                    for (size_t i = 0; i < new_size; ++i) {
                        d[i] = d[i + limb_shift];
                    }
                }
            } else {
                const uint64_t* src = d + limb_shift;
                for (size_t i = 0; i + 1 < new_size; ++i) {
                    d[i] = shrd64(src[i + 1], src[i], static_cast<unsigned int>(bit_shift));
                }
                d[new_size - 1] = src[new_size - 1] >> bit_shift;
            }
            if (new_size > 0 && d[new_size - 1] == 0) {
                --new_size;
            }
            res.m_size = static_cast<uint32_t>(new_size);
            if (new_size == 0) {
                res.m_sign = 0;
            }
            res.shrink_to_sbo_if_possible();
            return;
        }

        res.resize(new_size, 0);
        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.data(), a.data() + limb_shift, new_size);
        } else {
            const uint64_t* src = a.data() + limb_shift;
            for (size_t i = 0; i + 1 < new_size; ++i) {
                res.data()[i] = shrd64(src[i + 1], src[i], static_cast<unsigned int>(bit_shift));
            }
            res.data()[new_size - 1] = src[new_size - 1] >> bit_shift;
        }
        res.m_sign = (new_size > 0) ? a.m_sign : 0;
        res.normalize();
    }

    /// <summary>
    /// 位元非運算：~a = -a - 1。
    /// Bitwise NOT: ~a = -a - 1.
    /// 採快速進位/借位截斷技術，首個非溢位肢段後直接區塊拷貝，消除 $O(N)$ 逐肢段算術迴圈。
    /// </summary>
    /// <param name="res">輸出結果 / Output result</param>
    /// <param name="a">輸入數值 / Input operand a</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_not(BigIntStorage& res, const BigIntStorage& a) {
        if (a.m_size == 0 || a.m_sign == 0) {
            // ~0 = -1
            res.set_uint64(1, -1);
            return;
        }
        if (a.m_sign > 0) {
            // a > 0: ~a = -(a + 1)
            size_t k = 0;
            while (k < a.m_size && a.data()[k] == 0xFFFFFFFFFFFFFFFFULL) {
                ++k;
            }
            size_t new_size = (k == a.m_size) ? (a.m_size + 1) : a.m_size;

            if (&res == &a) {
                if (res.m_capacity < new_size) {
                    res.reserve(new_size);
                }
                for (size_t i = 0; i < k; ++i) {
                    res.data()[i] = 0;
                }
                if (k < a.m_size) {
                    res.data()[k] += 1;
                } else {
                    res.data()[a.m_size] = 1;
                }
                res.m_size = static_cast<uint32_t>(new_size);
                res.m_sign = -1;
                res.normalize();
                return;
            }

            res.resize(new_size, 0);
            for (size_t i = 0; i < k; ++i) {
                res.data()[i] = 0;
            }
            if (k < a.m_size) {
                res.data()[k] = a.data()[k] + 1;
                if (a.m_size > k + 1) {
                    BigIntStorage::copy_limbs(res.data() + k + 1, a.data() + k + 1, a.m_size - (k + 1));
                }
            } else {
                res.data()[a.m_size] = 1;
            }
            res.m_sign = -1;
            res.normalize();
            return;
        }

        // a < 0: a = -U => ~a = U - 1
        size_t k = 0;
        while (k < a.m_size && a.data()[k] == 0) {
            ++k;
        }
        if (&res == &a) {
            for (size_t i = 0; i < k; ++i) {
                res.data()[i] = 0xFFFFFFFFFFFFFFFFULL;
            }
            if (k < a.m_size) {
                res.data()[k] -= 1;
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }

        res.resize(a.m_size, 0);
        for (size_t i = 0; i < k; ++i) {
            res.data()[i] = 0xFFFFFFFFFFFFFFFFULL;
        }
        if (k < a.m_size) {
            res.data()[k] = a.data()[k] - 1;
            if (a.m_size > k + 1) {
                BigIntStorage::copy_limbs(res.data() + k + 1, a.data() + k + 1, a.m_size - (k + 1));
            }
        }
        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// 位元及運算：res = a & b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_and(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0 || b.m_sign == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t min_len = (a.m_size < b.m_size) ? a.m_size : b.m_size;
            if (&res == &a) {
                if (min_len == 1) {
                    res.data()[0] &= b.data()[0];
                    res.m_size = (res.data()[0] != 0) ? 1 : 0;
                    res.m_sign = (res.m_size != 0) ? 1 : 0;
                    return;
                }
                if (min_len == 2) {
                    res.data()[0] &= b.data()[0];
                    res.data()[1] &= b.data()[1];
                    res.m_size = (res.data()[1] != 0) ? 2 : ((res.data()[0] != 0) ? 1 : 0);
                    res.m_sign = (res.m_size != 0) ? 1 : 0;
                    return;
                }
                if (min_len == 4) {
                    res.data()[0] &= b.data()[0];
                    res.data()[1] &= b.data()[1];
                    res.data()[2] &= b.data()[2];
                    res.data()[3] &= b.data()[3];
                    size_t sz = 4;
                    while (sz > 0 && res.data()[sz - 1] == 0) --sz;
                    res.m_size = static_cast<uint32_t>(sz);
                    res.m_sign = (sz != 0) ? 1 : 0;
                    return;
                }
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] &= b.data()[i];
                }
                res.m_size = static_cast<uint32_t>(min_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] &= a.data()[i];
                }
                res.m_size = static_cast<uint32_t>(min_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(min_len, 0);
            for (size_t i = 0; i < min_len; ++i) {
                res.data()[i] = a.data()[i] & b.data()[i];
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            // a & b = a & ~(~b) = a & ~u
            BigIntStorage not_b;
            bitwise_not(not_b, b);
            if (&res == &a || &res == &b) {
                BigIntStorage tmp;
                bitwise_and(tmp, a, b);
                res = std::move(tmp);
                return;
            }
            res.resize(a.m_size, 0);
            size_t min_len = (a.m_size < not_b.m_size) ? a.m_size : not_b.m_size;
            for (size_t i = 0; i < min_len; ++i) {
                res.data()[i] = a.data()[i] & (~not_b.data()[i]);
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(res.data() + min_len, a.data() + min_len, a.m_size - min_len);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_and(res, b, a);
            return;
        }
        // a < 0 && b < 0: ~(a & b) = (~a) | (~b) => a & b = ~( (~a) | (~b) )
        BigIntStorage not_a, not_b, or_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_or(or_res, not_a, not_b);
        bitwise_not(res, or_res);
    }

    /// <summary>
    /// 位元或運算：res = a | b。
    /// 採重疊區間無分支向量化 + 長餘肢段區塊拷貝兩階段架構。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_or(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t min_len = (a.m_size < b.m_size) ? a.m_size : b.m_size;
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            if (&res == &a) {
                if (a.m_size == b.m_size) {
                    if (min_len == 1) {
                        res.data()[0] |= b.data()[0];
                        res.m_size = (res.data()[0] != 0) ? 1 : 0;
                        res.m_sign = (res.m_size != 0) ? 1 : 0;
                        return;
                    }
                    if (min_len == 2) {
                        res.data()[0] |= b.data()[0];
                        res.data()[1] |= b.data()[1];
                        res.m_size = (res.data()[1] != 0) ? 2 : ((res.data()[0] != 0) ? 1 : 0);
                        res.m_sign = (res.m_size != 0) ? 1 : 0;
                        return;
                    }
                    if (min_len == 4) {
                        res.data()[0] |= b.data()[0];
                        res.data()[1] |= b.data()[1];
                        res.data()[2] |= b.data()[2];
                        res.data()[3] |= b.data()[3];
                        res.m_size = 4;
                        res.m_sign = 1;
                        return;
                    }
                }
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] |= b.data()[i];
                }
                if (b.m_size > a.m_size) {
                    BigIntStorage::copy_limbs(res.data() + min_len, b.data() + min_len, b.m_size - min_len);
                }
                res.m_size = static_cast<uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] |= a.data()[i];
                }
                if (a.m_size > b.m_size) {
                    BigIntStorage::copy_limbs(res.data() + min_len, a.data() + min_len, a.m_size - min_len);
                }
                res.m_size = static_cast<uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(max_len, 0);
            for (size_t i = 0; i < min_len; ++i) {
                res.data()[i] = a.data()[i] | b.data()[i];
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(res.data() + min_len, a.data() + min_len, a.m_size - min_len);
            } else if (b.m_size > min_len) {
                BigIntStorage::copy_limbs(res.data() + min_len, b.data() + min_len, b.m_size - min_len);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        // De Morgan: ~(a | b) = (~a) & (~b) => a | b = ~( (~a) & (~b) )
        BigIntStorage not_a, not_b, and_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_and(and_res, not_a, not_b);
        bitwise_not(res, and_res);
    }

    /// <summary>
    /// 位元互斥或運算：res = a ^ b。
    /// 採重疊區間無分支向量化 + 長餘肢段區塊拷貝兩階段架構。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_xor(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t min_len = (a.m_size < b.m_size) ? a.m_size : b.m_size;
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            if (&res == &a) {
                if (a.m_size == b.m_size) {
                    if (min_len == 1) {
                        res.data()[0] ^= b.data()[0];
                        res.m_size = (res.data()[0] != 0) ? 1 : 0;
                        res.m_sign = (res.m_size != 0) ? 1 : 0;
                        return;
                    }
                    if (min_len == 2) {
                        res.data()[0] ^= b.data()[0];
                        res.data()[1] ^= b.data()[1];
                        res.m_size = (res.data()[1] != 0) ? 2 : ((res.data()[0] != 0) ? 1 : 0);
                        res.m_sign = (res.m_size != 0) ? 1 : 0;
                        return;
                    }
                    if (min_len == 4) {
                        res.data()[0] ^= b.data()[0];
                        res.data()[1] ^= b.data()[1];
                        res.data()[2] ^= b.data()[2];
                        res.data()[3] ^= b.data()[3];
                        size_t sz = 4;
                        while (sz > 0 && res.data()[sz - 1] == 0) --sz;
                        res.m_size = static_cast<uint32_t>(sz);
                        res.m_sign = (sz != 0) ? 1 : 0;
                        return;
                    }
                }
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] ^= b.data()[i];
                }
                if (b.m_size > a.m_size) {
                    BigIntStorage::copy_limbs(res.data() + min_len, b.data() + min_len, b.m_size - min_len);
                }
                res.m_size = static_cast<uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            if (&res == &b) {
                if (res.m_capacity < max_len) {
                    res.reserve(max_len);
                }
                for (size_t i = 0; i < min_len; ++i) {
                    res.data()[i] ^= a.data()[i];
                }
                if (a.m_size > b.m_size) {
                    BigIntStorage::copy_limbs(res.data() + min_len, a.data() + min_len, a.m_size - min_len);
                }
                res.m_size = static_cast<uint32_t>(max_len);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            res.resize(max_len, 0);
            for (size_t i = 0; i < min_len; ++i) {
                res.data()[i] = a.data()[i] ^ b.data()[i];
            }
            if (a.m_size > min_len) {
                BigIntStorage::copy_limbs(res.data() + min_len, a.data() + min_len, a.m_size - min_len);
            } else if (b.m_size > min_len) {
                BigIntStorage::copy_limbs(res.data() + min_len, b.data() + min_len, b.m_size - min_len);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            // a ^ b = ~(a ^ ~b)
            BigIntStorage not_b, xor_res;
            bitwise_not(not_b, b);
            bitwise_xor(xor_res, a, not_b);
            bitwise_not(res, xor_res);
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_xor(res, b, a);
            return;
        }
        // a < 0 && b < 0: a ^ b = (~a) ^ (~b)
        BigIntStorage not_a, not_b;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_xor(res, not_a, not_b);
    }
};

} // namespace detail
} // namespace numeric

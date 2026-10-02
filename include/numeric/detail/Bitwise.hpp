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
    /// limbs 級別的高效整區塊左移：res = a &lt;&lt; (limbs * 64)。
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
    /// 位元左移運算：res = a &lt;&lt; shift。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_left(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (shift == 0 || a.m_size == 0) {
            res = a;
            return;
        }
        if (&res == &a) {
            BigIntStorage tmp;
            shift_left(tmp, a, shift);
            res = std::move(tmp);
            return;
        }
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        size_t new_size = a.m_size + limb_shift + 1;
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
            uint64_t carry = 0;
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t cur = a.data()[i];
                res.data()[i + limb_shift] = (cur << bit_shift) | carry;
                carry = cur >> (64 - bit_shift);
            }
            res.data()[a.m_size + limb_shift] = carry;
        }
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// 位元右移運算：res = a &gt;&gt; shift（支援負數算術右移語意）。
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
            // 負數算術右移：a >> shift = ~((~a) >> shift) = - ((-a - 1) >> shift) - 1
            BigIntStorage u;
            BigIntStorage one_st; one_st.set_uint64(1, 1);
            BigIntStorage abs_a = a; abs_a.m_sign = 1;
            sub_signed(u, abs_a, one_st);

            BigIntStorage shifted_u;
            shift_right_positive(shifted_u, u, shift);

            BigIntStorage final_res;
            add_signed(final_res, shifted_u, one_st);
            final_res.m_sign = -1;
            final_res.normalize();
            res = std::move(final_res);
            return;
        }
        shift_right_positive(res, a, shift);
    }

    /// <summary>
    /// 正整數無符號位元右移運算。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_right_positive(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (&res == &a) {
            BigIntStorage tmp;
            shift_right_positive(tmp, a, shift);
            res = std::move(tmp);
            return;
        }
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        if (limb_shift >= a.m_size) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        size_t new_size = a.m_size - limb_shift;
        res.resize(new_size, 0);

        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.data(), a.data() + limb_shift, new_size);
        } else {
            for (size_t i = 0; i < new_size; ++i) {
                uint64_t cur = a.data()[i + limb_shift];
                uint64_t next = (i + limb_shift + 1 < a.m_size) ? a.data()[i + limb_shift + 1] : 0;
                res.data()[i] = (cur >> bit_shift) | (next << (64 - bit_shift));
            }
        }
        res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        res.normalize();
    }

    /// <summary>
    /// 位元非運算：~a = -a - 1。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">輸入數值</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_not(BigIntStorage& res, const BigIntStorage& a) {
        BigIntStorage one_st; one_st.set_uint64(1, 1);
        BigIntStorage tmp;
        add_signed(tmp, a, one_st);
        tmp.m_sign = -tmp.m_sign;
        tmp.normalize();
        res = std::move(tmp);
    }

    /// <summary>
    /// 位元及運算：res = a &amp; b。
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
            res.resize(a.m_size, 0);
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t nu = (i < not_b.m_size) ? not_b.data()[i] : 0;
                res.data()[i] = a.data()[i] & (~nu);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_and(res, b, a);
            return;
        }
        // a < 0 && b < 0: ~(a & b) = (~a) | (~b)
        BigIntStorage not_a, not_b, or_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_or(or_res, not_a, not_b);
        bitwise_not(res, or_res);
    }

    /// <summary>
    /// 位元或運算：res = a | b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_or(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            res.resize(max_len, 0);
            for (size_t i = 0; i < max_len; ++i) {
                uint64_t av = (i < a.m_size) ? a.data()[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.data()[i] : 0;
                res.data()[i] = av | bv;
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        // De Morgan: ~(a | b) = (~a) & (~b)
        BigIntStorage not_a, not_b, and_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_and(and_res, not_a, not_b);
        bitwise_not(res, and_res);
    }

    /// <summary>
    /// 位元互斥或運算：res = a ^ b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_xor(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            res.resize(max_len, 0);
            for (size_t i = 0; i < max_len; ++i) {
                uint64_t av = (i < a.m_size) ? a.data()[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.data()[i] : 0;
                res.data()[i] = av ^ bv;
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

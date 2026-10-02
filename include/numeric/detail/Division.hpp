#pragma once

// Division.hpp
// Arbitrary-precision integer division algorithms for CPP-BigInt:
// Knuth Algorithm D, Burnikel-Ziegler 3/2 and 2/1 divide-and-conquer division.
// Part of the numeric::detail modular core.

#include "Multiplication.hpp"
#include <algorithm>
#include <vector>
#include <stdexcept>

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 除法運算核心類別，繼承自 BigIntMultiplication。
/// 支援單肢段快除、Knuth Algorithm D 長除法與 Burnikel-Ziegler 分治除法。
/// </summary>
struct BigIntDivision : public BigIntMultiplication {
    static constexpr size_t BZ_THRESHOLD = 128; // Tuned threshold for Burnikel-Ziegler division

    /// <summary>
    /// Knuth Algorithm D 原地長除法（採用 Scratch Buffer 達成 16,384 位元內零 Heap 配置）。
    /// 依據 q_out 與 r_out 是否為空，完全消除不必要的商或餘數陣列存取與正規化開銷。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core_knuth(
        BigIntStorage* q_out, BigIntStorage* r_out,
        const BigIntStorage& u, const BigIntStorage& v)
    {
        if (v.m_size == 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0 || u.m_size < v.m_size) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = (u.m_size > 0 ? 1 : 0); }
            return;
        }
        int cmp = compare_unsigned(u.data(), u.m_size, v.data(), v.m_size);
        if (cmp < 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out) { q_out->set_uint64(1, 1); }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        size_t n = v.m_size;
        size_t m = u.m_size - n;

        // D1: 正規化 (shift left by s bits)
        int s = clz64(v.data()[n - 1]);
        size_t total_scratch = (n + 1) + (u.m_size + 2);
        uint64_t stack_scratch[512];
        uint64_t* scratch_ptr = stack_scratch;
        std::vector<uint64_t> heap_scratch;
        if (total_scratch > 512) {
            heap_scratch.resize(total_scratch);
            scratch_ptr = heap_scratch.data();
        }

        uint64_t* vn = scratch_ptr;
        uint64_t* un = scratch_ptr + (n + 1);

        if (s == 0) {
            BigIntStorage::copy_limbs(vn, v.data(), n);
            BigIntStorage::copy_limbs(un, u.data(), u.m_size);
            un[u.m_size] = 0;
            un[u.m_size + 1] = 0;
        } else {
            uint64_t carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t cur = v.data()[i];
                vn[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            vn[n] = carry;

            carry = 0;
            for (size_t i = 0; i < u.m_size; ++i) {
                uint64_t cur = u.data()[i];
                un[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            un[u.m_size] = carry;
            un[u.m_size + 1] = 0;
        }

        if (q_out) {
            q_out->resize(m + 1, 0);
        }

        uint64_t v_hi = vn[n - 1];
        uint64_t v_lo = vn[n - 2];

        // D2~D7: 主迴圈
        for (size_t k = m + 1; k > 0; --k) {
            size_t j = k - 1;
            uint64_t u_hi = un[j + n];
            uint64_t u_mid = un[j + n - 1];
            uint64_t u_lo = (j + n >= 2) ? un[j + n - 2] : 0;

            uint64_t q_hat = 0;
            uint64_t r_hat = 0;

            if (u_hi == v_hi) {
                q_hat = 0xFFFFFFFFFFFFFFFFULL;
                r_hat = u_mid + v_hi;
                if (r_hat >= v_hi) {
                    while (true) {
                        uint64_t p_hi = 0;
                        uint64_t p_lo = mul64_wide(q_hat, v_lo, p_hi);
                        if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                            --q_hat;
                            r_hat += v_hi;
                            if (r_hat < v_hi) break;
                        } else {
                            break;
                        }
                    }
                }
            } else {
                q_hat = div128_64(u_hi, u_mid, v_hi, r_hat);
                while (true) {
                    uint64_t p_hi = 0;
                    uint64_t p_lo = mul64_wide(q_hat, v_lo, p_hi);
                    if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                        --q_hat;
                        r_hat += v_hi;
                        if (r_hat < v_hi) break;
                    } else {
                        break;
                    }
                }
            }

            // D4: 乘並減
            uint64_t carry = 0;
            uint64_t borrow = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t p_hi = 0;
                uint64_t p_lo = mul64_wide(q_hat, vn[i], p_hi);
                uint64_t p_full = p_lo + carry;
                uint64_t c1 = (p_full < p_lo) ? 1 : 0;
                carry = p_hi + c1;

                uint64_t cur = un[j + i];
                uint64_t diff = cur - borrow;
                uint64_t b1 = (cur < borrow) ? 1 : 0;
                uint64_t diff2 = diff - p_full;
                uint64_t b2 = (diff < p_full) ? 1 : 0;
                borrow = b1 + b2;
                un[j + i] = diff2;
            }

            uint64_t cur = un[j + n];
            uint64_t diff = cur - borrow;
            uint64_t b1 = (cur < borrow) ? 1 : 0;
            uint64_t diff2 = diff - carry;
            uint64_t b2 = (diff < carry) ? 1 : 0;
            un[j + n] = diff2;

            // D5: 判斷是否需要回加
            if (b1 + b2 > 0) {
                --q_hat;
                uint64_t add_carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t val = un[j + i];
                    uint64_t sum = val + vn[i] + add_carry;
                    add_carry = (sum < val || (add_carry && sum == val)) ? 1 : 0;
                    un[j + i] = sum;
                }
                un[j + n] += add_carry;
            }

            if (q_out) {
                q_out->data()[j] = q_hat;
            }
        }

        if (q_out) {
            q_out->m_sign = 1;
            q_out->normalize();
        }

        // D8: 去正規化餘數
        if (r_out) {
            r_out->resize(n, 0);
            if (s > 0) {
                for (size_t i = 0; i < n; ++i) {
                    uint64_t c = un[i];
                    uint64_t next = (i + 1 <= n) ? un[i + 1] : 0;
                    r_out->data()[i] = (c >> s) | (next << (64 - s));
                }
            } else {
                BigIntStorage::copy_limbs(r_out->data(), un, n);
            }
            r_out->m_sign = 1;
            r_out->normalize();
        }
    }

    /// <summary>
    /// Burnikel–Ziegler div3by2: 以 2k-limb 除數 b 除 3k-limb 被除數 a。
    /// 產出 k-limb 商 q 與 2k-limb 餘數 r。要求 a < b * beta^k。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div3by2(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b, size_t k)
    {
        BigIntStorage b1, b0;
        b1.resize(k, 0);
        BigIntStorage::copy_limbs(b1.data(), b.data() + k, k);
        b1.m_sign = 1;
        b1.normalize();

        b0.resize(k, 0);
        BigIntStorage::copy_limbs(b0.data(), b.data(), k);
        b0.m_sign = 1;
        b0.normalize();

        BigIntStorage a_hi, a_lo;
        if (a.m_size > k) {
            size_t hi_len = a.m_size - k;
            a_hi.resize(hi_len, 0);
            BigIntStorage::copy_limbs(a_hi.data(), a.data() + k, hi_len);
            a_hi.m_sign = 1;
            a_hi.normalize();

            a_lo.resize(k, 0);
            BigIntStorage::copy_limbs(a_lo.data(), a.data(), k);
            a_lo.m_sign = 1;
            a_lo.normalize();
        } else {
            a_lo = a;
        }

        BigIntStorage q_hat, r_hat;
        if (a_hi.m_size == 0) {
            q_hat.m_size = 0;
            q_hat.m_sign = 0;
            r_hat.m_size = 0;
            r_hat.m_sign = 0;
        } else {
            bool a_hi_ge_b1_beta_k = false;
            if (a_hi.m_size > k + b1.m_size) {
                a_hi_ge_b1_beta_k = true;
            } else if (a_hi.m_size == k + b1.m_size) {
                a_hi_ge_b1_beta_k = (compare_unsigned(a_hi.data() + k, b1.m_size, b1.data(), b1.m_size) >= 0);
            }

            if (a_hi_ge_b1_beta_k) {
                q_hat.resize(k, 0xFFFFFFFFFFFFFFFFULL);
                q_hat.m_sign = 1;

                BigIntStorage b1_shifted;
                shift_left_limbs(b1_shifted, b1, k);
                BigIntStorage diff;
                sub_magnitude_core(diff, a_hi, b1_shifted);
                add_unsigned(r_hat, diff, b1);
                r_hat.m_sign = 1;
            } else {
                bz_div2by1(&q_hat, &r_hat, a_hi, b1);
            }
        }

        BigIntStorage d;
        mul_core(d, q_hat, b0);

        BigIntStorage r_prime;
        shift_left_limbs(r_prime, r_hat, k);
        if (a_lo.m_size > 0) {
            BigIntStorage tmp;
            add_unsigned(tmp, r_prime, a_lo);
            tmp.m_sign = 1;
            r_prime = std::move(tmp);
        }

        while (compare_unsigned(r_prime.data(), r_prime.m_size, d.data(), d.m_size) < 0) {
            BigIntStorage one; one.set_uint64(1, 1);
            BigIntStorage q_next;
            sub_magnitude_core(q_next, q_hat, one);
            q_next.m_sign = (q_next.m_size > 0) ? 1 : 0;
            q_hat = std::move(q_next);

            BigIntStorage r_next;
            add_unsigned(r_next, r_prime, b);
            r_next.m_sign = 1;
            r_prime = std::move(r_next);
        }

        BigIntStorage r_final;
        sub_magnitude_core(r_final, r_prime, d);
        r_final.m_sign = (r_final.m_size > 0) ? 1 : 0;

        if (q) *q = std::move(q_hat);
        if (r) *r = std::move(r_final);
    }

    /// <summary>
    /// Burnikel–Ziegler div2by1: 以 n-limb 除數 b 除 2n-limb 被除數 a。
    /// 要求 a < b * beta^n，n 為偶數。
    /// 遞迴調用 div3by2，以 Karatsuba 乘法降低計算複雜度至 O(M(N) log N)。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div2by1(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b)
    {
        size_t n = b.m_size;
        if (n < BZ_THRESHOLD || (n % 2 != 0)) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }

        size_t k = n / 2;
        BigIntStorage a_hi, a_lo;
        if (a.m_size > k) {
            size_t hi_len = a.m_size - k;
            a_hi.resize(hi_len, 0);
            BigIntStorage::copy_limbs(a_hi.data(), a.data() + k, hi_len);
            a_hi.m_sign = 1;
            a_hi.normalize();

            a_lo.resize(k, 0);
            BigIntStorage::copy_limbs(a_lo.data(), a.data(), k);
            a_lo.m_sign = 1;
            a_lo.normalize();
        } else {
            a_lo = a;
        }

        BigIntStorage q1, r1;
        bz_div3by2(&q1, &r1, a_hi, b, k);

        BigIntStorage a_step2;
        shift_left_limbs(a_step2, r1, k);
        if (a_lo.m_size > 0) {
            BigIntStorage tmp;
            add_unsigned(tmp, a_step2, a_lo);
            tmp.m_sign = 1;
            a_step2 = std::move(tmp);
        }

        BigIntStorage q0, r0;
        bz_div3by2(&q0, &r0, a_step2, b, k);

        if (q) {
            shift_left_limbs(*q, q1, k);
            if (q0.m_size > 0) {
                BigIntStorage tmp;
                add_unsigned(tmp, *q, q0);
                tmp.m_sign = 1;
                *q = std::move(tmp);
            }
        }
        if (r) {
            *r = std::move(r0);
        }
    }

    /// <summary>
    /// 核心長除法與取模派發入口。
    /// 支援單 limb 快速路徑、Knuth Algorithm D 與 Burnikel–Ziegler 分治除法。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core(
        BigIntStorage* q_out, BigIntStorage* r_out,
        const BigIntStorage& u, const BigIntStorage& v)
    {
        if (v.m_size == 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }
        int cmp = compare_unsigned(u.data(), u.m_size, v.data(), v.m_size);
        if (cmp < 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out) { q_out->set_uint64(1, 1); }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        // 單 limb 快速除法路徑
        if (v.m_size == 1) {
            uint64_t divisor = v.data()[0];
            if (q_out) q_out->resize(u.m_size, 0);
            uint64_t rem = 0;
            for (size_t i = u.m_size; i > 0; --i) {
                uint64_t next_rem = 0;
                uint64_t q_limb = div128_64(rem, u.data()[i - 1], divisor, next_rem);
                if (q_out) q_out->data()[i - 1] = q_limb;
                rem = next_rem;
            }
            if (q_out) {
                q_out->m_sign = 1;
                q_out->normalize();
            }
            if (r_out) {
                if (rem != 0) {
                    r_out->set_uint64(rem, 1);
                } else {
                    r_out->m_size = 0;
                    r_out->m_sign = 0;
                }
            }
            return;
        }

        // 小於 BZ_THRESHOLD (64 limbs, 4096-bit) 採 Knuth Algorithm D 長除法
        if (v.m_size < BZ_THRESHOLD) {
            div_mod_core_knuth(q_out, r_out, u, v);
            return;
        }

        // Burnikel–Ziegler 大數分治除法路徑
        size_t s = v.m_size;
        size_t m_pow2 = 2;
        while (m_pow2 * BZ_THRESHOLD <= s) {
            m_pow2 <<= 1;
        }
        size_t j = (s + m_pow2 - 1) / m_pow2;
        size_t n = j * m_pow2; // n is always even and >= s

        size_t n_bits = n * 64;
        size_t v_bits = (v.m_size - 1) * 64 + (64 - clz64(v.data()[v.m_size - 1]));
        size_t sigma = (n_bits >= v_bits) ? (n_bits - v_bits) : 0;

        BigIntStorage v_shifted, u_shifted;
        if (sigma > 0) {
            shift_left(v_shifted, v, sigma);
            shift_left(u_shifted, u, sigma);
        } else {
            v_shifted = v;
            u_shifted = u;
        }

        size_t u_bits = (u_shifted.m_size > 0) 
            ? ((u_shifted.m_size - 1) * 64 + (64 - clz64(u_shifted.data()[u_shifted.m_size - 1])))
            : 0;
        size_t t = (u_bits + n_bits) / n_bits;
        if (t < 2) t = 2;

        auto get_block = [&](BigIntStorage& out, size_t block_idx) {
            size_t start = block_idx * n;
            if (start >= u_shifted.m_size) {
                out.m_size = 0;
                out.m_sign = 0;
                return;
            }
            size_t len = (std::min)(n, u_shifted.m_size - start);
            out.resize(len, 0);
            BigIntStorage::copy_limbs(out.data(), u_shifted.data() + start, len);
            out.m_sign = 1;
            out.normalize();
        };

        BigIntStorage r_cur;
        get_block(r_cur, t - 1);

        BigIntStorage q_total;
        for (size_t i = t - 1; i > 0; --i) {
            size_t block_idx = i - 1;
            BigIntStorage a_block;
            get_block(a_block, block_idx);

            BigIntStorage z;
            shift_left_limbs(z, r_cur, n);
            if (a_block.m_size > 0) {
                BigIntStorage tmp;
                add_unsigned(tmp, z, a_block);
                tmp.m_sign = 1;
                z = std::move(tmp);
            }

            BigIntStorage q_step, r_step;
            bz_div2by1(&q_step, &r_step, z, v_shifted);
            r_cur = std::move(r_step);

            if (q_out) {
                shift_left_limbs(q_total, q_total, n);
                if (q_step.m_size > 0) {
                    BigIntStorage tmp;
                    add_unsigned(tmp, q_total, q_step);
                    tmp.m_sign = 1;
                    q_total = std::move(tmp);
                }
            }
        }

        if (q_out) {
            *q_out = std::move(q_total);
            q_out->m_sign = (q_out->m_size > 0) ? 1 : 0;
            q_out->normalize();
        }
        if (r_out) {
            if (sigma > 0) {
                shift_right(*r_out, r_cur, sigma);
            } else {
                *r_out = std::move(r_cur);
            }
            r_out->m_sign = (r_out->m_size > 0) ? 1 : 0;
            r_out->normalize();
        }
    }

    /// <summary>
    /// 商餘同求長除法介面。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_qr(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
    }

    /// <summary>
    /// 僅求商長除法介面（省略餘數處理）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_q(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, nullptr, u, v);
    }

    /// <summary>
    /// 僅求餘數取模介面（省略商輸出處理）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_r(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(nullptr, &r, u, v);
    }

    /// <summary>
    /// 帶符號商餘同求除法運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_qr_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
        if (q.m_size > 0) {
            q.m_sign = static_cast<int8_t>(u.m_sign * v.m_sign);
        }
        if (r.m_size > 0) {
            r.m_sign = u.m_sign;
        }
    }

    /// <summary>
    /// 帶符號僅求商除法運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_q_signed(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, nullptr, u, v);
        if (q.m_size > 0) {
            q.m_sign = static_cast<int8_t>(u.m_sign * v.m_sign);
        }
    }

    /// <summary>
    /// 帶符號僅求餘數取模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_r_signed(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(nullptr, &r, u, v);
        if (r.m_size > 0) {
            r.m_sign = u.m_sign;
        }
    }

    /// <summary>
    /// 傳統相容介面：帶符號除法與模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_qr_signed(q, r, u, v);
    }

    /// <summary>
    /// 傳統相容介面：無符號除法與模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
    }
};

} // namespace detail
} // namespace numeric

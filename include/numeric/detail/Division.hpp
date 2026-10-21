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
    /// 乘法分段輔助函式：out = a * b。out 容量至少為 a_len + b_len。
    /// 依運算元大小自動派發至 Schoolbook, Karatsuba 或 Toom-3，並從 arena 配置暫存空間。
    /// </summary>
    static void mul_slice_helper(
        uint64_t* NUMERIC_RESTRICT out,
        const uint64_t* a, size_t a_len,
        const uint64_t* b, size_t b_len,
        ScratchArena& arena) noexcept
    {
        size_t n = (a_len > b_len) ? a_len : b_len;
        if (a_len == 0 || b_len == 0) {
            BigIntStorage::zero_limbs(out, a_len + b_len);
            return;
        }
        if (n < KARATSUBA_THRESHOLD || a_len < 4 || b_len < 4 ||
            a_len >= 2 * b_len || b_len >= 2 * a_len) {
            mul_schoolbook_raw(out, a, a_len, b, b_len);
        } else if (n < TOOM3_THRESHOLD) {
            uint64_t* scratch = arena.allocate(8 * n + 128);
            mul_karatsuba_raw(out, a, a_len, b, b_len, scratch);
        } else {
            uint64_t* scratch = arena.allocate(48 * n + 2048);
            mul_toom3_raw(out, a, a_len, b, b_len, scratch);
        }
    }

    /// <summary>
    /// Knuth Algorithm D 原地長除法底層 Raw 介面。
    /// 依據 q_out 與 r_out 是否為空，完全消除不必要的商或餘數陣列存取與正規化開銷。
    /// scratch 必須至少具備 (v_len + 1) + (u_len + 2) 之容量。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_knuth_raw(
        uint64_t* q_out, size_t* q_len,
        uint64_t* r_out, size_t* r_len,
        const uint64_t* u, size_t u_len,
        const uint64_t* v, size_t v_len,
        uint64_t* scratch) noexcept
    {
        if (v_len == 0) {
            return;
        }
        if (u_len == 0 || u_len < v_len) {
            if (q_len) *q_len = 0;
            if (r_out && u_len > 0) {
                BigIntStorage::copy_limbs(r_out, u, u_len);
            }
            if (r_len) *r_len = u_len;
            return;
        }

        int cmp = compare_unsigned(u, u_len, v, v_len);
        if (cmp < 0) {
            if (q_len) *q_len = 0;
            if (r_out && u_len > 0) {
                BigIntStorage::copy_limbs(r_out, u, u_len);
            }
            if (r_len) *r_len = u_len;
            return;
        }
        if (cmp == 0) {
            if (q_out) q_out[0] = 1;
            if (q_len) *q_len = 1;
            if (r_len) *r_len = 0;
            return;
        }

        if (v_len == 1) {
            uint64_t divisor = v[0];
            uint64_t rem = 0;
            if (q_out) {
                for (size_t i = u_len; i > 0; --i) {
                    uint64_t next_rem = 0;
                    q_out[i - 1] = div128_64(rem, u[i - 1], divisor, next_rem);
                    rem = next_rem;
                }
                size_t ql = u_len;
                while (ql > 0 && q_out[ql - 1] == 0) --ql;
                if (q_len) *q_len = ql;
            } else {
                for (size_t i = u_len; i > 0; --i) {
                    uint64_t next_rem = 0;
                    div128_64(rem, u[i - 1], divisor, next_rem);
                    rem = next_rem;
                }
                if (q_len) *q_len = 0;
            }
            if (r_out) {
                r_out[0] = rem;
                if (r_len) *r_len = (rem != 0 ? 1 : 0);
            } else if (r_len) {
                *r_len = (rem != 0 ? 1 : 0);
            }
            return;
        }

        size_t n = v_len;
        size_t m = u_len - n;

        // D1: 正規化 (shift left by s bits)
        int s = clz64(v[n - 1]);
        uint64_t* vn = scratch;
        uint64_t* un = scratch + (n + 1);

        if (s == 0) {
            BigIntStorage::copy_limbs(vn, v, n);
            BigIntStorage::copy_limbs(un, u, u_len);
            un[u_len] = 0;
            un[u_len + 1] = 0;
        } else {
            uint64_t carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t cur = v[i];
                vn[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            vn[n] = carry;

            carry = 0;
            for (size_t i = 0; i < u_len; ++i) {
                uint64_t cur = u[i];
                un[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            un[u_len] = carry;
            un[u_len + 1] = 0;
        }

        uint64_t v_hi = vn[n - 1];
        uint64_t v_lo = vn[n - 2];

        // D2~D7: 主迴圈
        for (size_t k_idx = m + 1; k_idx > 0; --k_idx) {
            size_t j = k_idx - 1;
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
            uint64_t borrow = submul_1_raw(un + j, vn, n, q_hat);
            uint64_t cur = un[j + n];
            uint64_t diff = cur - borrow;
            uint64_t b = (cur < borrow) ? 1 : 0;
            un[j + n] = diff;

            // D5: 判斷是否需要回加
            if (b > 0) {
                --q_hat;
                uint64_t add_carry = add_to_raw(un + j, n, vn, n);
                un[j + n] += add_carry;
            }

            if (q_out) {
                q_out[j] = q_hat;
            }
        }

        if (q_out && q_len) {
            size_t ql = m + 1;
            while (ql > 0 && q_out[ql - 1] == 0) --ql;
            *q_len = ql;
        }

        // D8: 去正規化餘數
        if (r_out) {
            if (s > 0) {
                for (size_t i = 0; i < n; ++i) {
                    uint64_t c = un[i];
                    uint64_t next = (i + 1 <= n) ? un[i + 1] : 0;
                    r_out[i] = (c >> s) | (next << (64 - s));
                }
            } else {
                BigIntStorage::copy_limbs(r_out, un, n);
            }
            if (r_len) {
                size_t rl = n;
                while (rl > 0 && r_out[rl - 1] == 0) --rl;
                *r_len = rl;
            }
        } else if (r_len) {
            *r_len = 0;
        }
    }

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
        size_t total_scratch = (n + 1) + (u.m_size + 2);
        uint64_t stack_scratch[512];
        uint64_t* scratch_ptr = stack_scratch;
        std::vector<uint64_t> heap_scratch;
        if (total_scratch > 512) {
            heap_scratch.resize(total_scratch);
            scratch_ptr = heap_scratch.data();
        }

        if (q_out) {
            q_out->resize(m + 1, 0);
        }
        if (r_out) {
            r_out->resize(n, 0);
        }

        size_t ql = 0, rl = 0;
        div_knuth_raw(
            q_out ? q_out->data() : nullptr, &ql,
            r_out ? r_out->data() : nullptr, &rl,
            u.data(), u.m_size,
            v.data(), v.m_size,
            scratch_ptr);

        if (q_out) {
            q_out->m_size = static_cast<uint32_t>(ql);
            q_out->m_sign = (ql > 0 ? 1 : 0);
            q_out->normalize();
        }
        if (r_out) {
            r_out->m_size = static_cast<uint32_t>(rl);
            r_out->m_sign = (rl > 0 ? 1 : 0);
            r_out->normalize();
        }
    }

    /// <summary>
    /// Burnikel–Ziegler div3by2 raw: 以 2k-limb 除數 b 除 3k-limb 被除數 a。
    /// 產出 k-limb 商 q 與 2k-limb 餘數 r。要求 a < b * beta^k。
    /// 搭配 ScratchArena 與指針切片達成全程 0 堆積記憶體配置。
    /// </summary>
    static void bz_div3by2_raw(
        uint64_t* NUMERIC_RESTRICT q, size_t* q_len,
        uint64_t* NUMERIC_RESTRICT r, size_t* r_len,
        const uint64_t* a, size_t a_len,
        const uint64_t* b, size_t k,
        ScratchArena& arena);

    /// <summary>
    /// Burnikel–Ziegler div2by1 raw: 以 n-limb 除數 b 除 2n-limb 被除數 a。
    /// 要求 a < b * beta^n，n 為偶數。
    /// 遞迴調用 div3by2，以 Karatsuba/Toom-3 乘法降低計算複雜度。
    /// 搭配 ScratchArena 達成全程 0 堆積記憶體配置。
    /// </summary>
    static void bz_div2by1_raw(
        uint64_t* NUMERIC_RESTRICT q, size_t* q_len,
        uint64_t* NUMERIC_RESTRICT r, size_t* r_len,
        const uint64_t* a, size_t a_len,
        const uint64_t* b, size_t n,
        ScratchArena& arena);

    /// <summary>
    /// Burnikel–Ziegler div3by2: 以 2k-limb 除數 b 除 3k-limb 被除數 a。
    /// 產出 k-limb 商 q 與 2k-limb 餘數 r。要求 a < b * beta^k。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div3by2(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b, size_t k)
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }
#endif
        auto& arena = ScratchArena::instance();
        ScratchArena::Scope scope(arena);

        uint64_t* q_buf = arena.allocate(k + 1);
        uint64_t* r_buf = arena.allocate(2 * k + 1);
        size_t ql = 0, rl = 0;

        bz_div3by2_raw(q_buf, &ql, r_buf, &rl, a.data(), a.m_size, b.data(), k, arena);

        if (q) {
            q->resize(ql, 0);
            if (ql > 0) BigIntStorage::copy_limbs(q->data(), q_buf, ql);
            q->m_size = static_cast<uint32_t>(ql);
            q->m_sign = (ql > 0) ? 1 : 0;
            q->normalize();
        }
        if (r) {
            r->resize(rl, 0);
            if (rl > 0) BigIntStorage::copy_limbs(r->data(), r_buf, rl);
            r->m_size = static_cast<uint32_t>(rl);
            r->m_sign = (rl > 0) ? 1 : 0;
            r->normalize();
        }
    }

    /// <summary>
    /// Burnikel–Ziegler div2by1: 以 n-limb 除數 b 除 2n-limb 被除數 a。
    /// 要求 a < b * beta^n，n 為偶數。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div2by1(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b)
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }
#endif
        size_t n = b.m_size;
        if (n < BZ_THRESHOLD || (n % 2 != 0)) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }

        auto& arena = ScratchArena::instance();
        ScratchArena::Scope scope(arena);

        uint64_t* q_buf = arena.allocate(n + 1);
        uint64_t* r_buf = arena.allocate(n + 1);
        size_t ql = 0, rl = 0;

        bz_div2by1_raw(q_buf, &ql, r_buf, &rl, a.data(), a.m_size, b.data(), n, arena);

        if (q) {
            q->resize(ql, 0);
            if (ql > 0) BigIntStorage::copy_limbs(q->data(), q_buf, ql);
            q->m_size = static_cast<uint32_t>(ql);
            q->m_sign = (ql > 0) ? 1 : 0;
            q->normalize();
        }
        if (r) {
            r->resize(rl, 0);
            if (rl > 0) BigIntStorage::copy_limbs(r->data(), r_buf, rl);
            r->m_size = static_cast<uint32_t>(rl);
            r->m_sign = (rl > 0) ? 1 : 0;
            r->normalize();
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

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q_out, r_out, u, v);
            return;
        }
#endif

        // 小於 BZ_THRESHOLD (128 limbs, 8192-bit) 採 Knuth Algorithm D 長除法
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

        auto& arena = ScratchArena::instance();
        ScratchArena::Scope scope(arena);

        uint64_t* z = arena.allocate(2 * n + 1);
        uint64_t* q_step = arena.allocate(n + 1);
        uint64_t* r_cur = arena.allocate(n + 1);
        uint64_t* r_next = arena.allocate(n + 1);

        // 初始化 r_cur 為 u_shifted 的最高 block (block t - 1)
        BigIntStorage::zero_limbs(r_cur, n + 1);
        size_t top_start = (t - 1) * n;
        size_t r_cur_len = 0;
        if (top_start < u_shifted.m_size) {
            r_cur_len = (std::min)(n, u_shifted.m_size - top_start);
            BigIntStorage::copy_limbs(r_cur, u_shifted.data() + top_start, r_cur_len);
            while (r_cur_len > 0 && r_cur[r_cur_len - 1] == 0) --r_cur_len;
        }

        if (q_out) {
            q_out->resize((t - 1) * n, 0);
            BigIntStorage::zero_limbs(q_out->data(), (t - 1) * n);
        }

        for (size_t i = t - 1; i > 0; --i) {
            size_t block_idx = i - 1;
            size_t start = block_idx * n;
            size_t a_block_len = 0;
            if (start < u_shifted.m_size) {
                a_block_len = (std::min)(n, u_shifted.m_size - start);
            }

            BigIntStorage::zero_limbs(z, 2 * n + 1);
            if (a_block_len > 0) {
                BigIntStorage::copy_limbs(z, u_shifted.data() + start, a_block_len);
            }
            if (r_cur_len > 0) {
                BigIntStorage::copy_limbs(z + n, r_cur, r_cur_len);
            }
            size_t z_len = n + r_cur_len;
            while (z_len > 0 && z[z_len - 1] == 0) --z_len;

            size_t q_step_len = 0;
            size_t r_next_len = 0;
            bz_div2by1_raw(q_step, &q_step_len, r_next, &r_next_len, z, z_len, v_shifted.data(), n, arena);

            BigIntStorage::zero_limbs(r_cur, n + 1);
            if (r_next_len > 0) {
                BigIntStorage::copy_limbs(r_cur, r_next, r_next_len);
            }
            r_cur_len = r_next_len;

            if (q_out) {
                if (q_step_len > 0) {
                    BigIntStorage::copy_limbs(q_out->data() + (i - 1) * n, q_step, q_step_len);
                }
            }
        }

        if (q_out) {
            q_out->m_size = static_cast<uint32_t>((t - 1) * n);
            q_out->m_sign = (q_out->m_size > 0) ? 1 : 0;
            q_out->normalize();
        }
        if (r_out) {
            BigIntStorage r_temp;
            r_temp.resize(r_cur_len, 0);
            if (r_cur_len > 0) {
                BigIntStorage::copy_limbs(r_temp.data(), r_cur, r_cur_len);
                r_temp.m_size = static_cast<uint32_t>(r_cur_len);
                r_temp.m_sign = 1;
            } else {
                r_temp.m_size = 0;
                r_temp.m_sign = 0;
            }
            if (sigma > 0) {
                shift_right(*r_out, r_temp, sigma);
            } else {
                *r_out = std::move(r_temp);
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

// ----------------------------------------------------------------------------
// Burnikel–Ziegler 遞迴函式實作
// ----------------------------------------------------------------------------

inline void BigIntDivision::bz_div3by2_raw(
    uint64_t* NUMERIC_RESTRICT q, size_t* q_len,
    uint64_t* NUMERIC_RESTRICT r, size_t* r_len,
    const uint64_t* a, size_t a_len,
    const uint64_t* b, size_t k,
    ScratchArena& arena)
{
    if (a_len == 0) {
        if (q) BigIntStorage::zero_limbs(q, k);
        if (q_len) *q_len = 0;
        if (r) BigIntStorage::zero_limbs(r, 2 * k);
        if (r_len) *r_len = 0;
        return;
    }

    const uint64_t* b1 = b + k;
    const uint64_t* b0 = b;
    size_t b0_len = k;
    while (b0_len > 0 && b0[b0_len - 1] == 0) --b0_len;

    const uint64_t* a_hi = (a_len > k) ? (a + k) : nullptr;
    size_t a_hi_len = (a_len > k) ? (a_len - k) : 0;
    while (a_hi_len > 0 && a_hi[a_hi_len - 1] == 0) --a_hi_len;

    const uint64_t* a_lo = a;
    size_t a_lo_len = (a_len < k) ? a_len : k;
    while (a_lo_len > 0 && a_lo[a_lo_len - 1] == 0) --a_lo_len;

    ScratchArena::Scope scope(arena);

    uint64_t* q_hat = arena.allocate(k + 1);
    uint64_t* r_hat = arena.allocate(2 * k + 2);
    BigIntStorage::zero_limbs(q_hat, k + 1);
    BigIntStorage::zero_limbs(r_hat, 2 * k + 2);
    size_t q_hat_len = 0;
    size_t r_hat_len = 0;

    if (a_hi_len == 0) {
        q_hat_len = 0;
        r_hat_len = 0;
    } else {
        bool a_hi_ge_b1_beta_k = false;
        if (a_hi_len > 2 * k) {
            a_hi_ge_b1_beta_k = true;
        } else if (a_hi_len == 2 * k) {
            a_hi_ge_b1_beta_k = (compare_unsigned(a_hi + k, k, b1, k) >= 0);
        }

        if (a_hi_ge_b1_beta_k) {
            for (size_t i = 0; i < k; ++i) {
                q_hat[i] = 0xFFFFFFFFFFFFFFFFULL;
            }
            q_hat_len = k;

            // r_hat = (a_hi - b1 * beta^k) + b1
            size_t lo_copy = (a_hi_len < k) ? a_hi_len : k;
            if (lo_copy > 0) {
                BigIntStorage::copy_limbs(r_hat, a_hi, lo_copy);
            }
            uint64_t carry = add_to_raw(r_hat, k, b1, k);
            r_hat[k] = carry;
            if (a_hi_len > k) {
                size_t hi_part_len = a_hi_len - k;
                if (compare_unsigned(a_hi + k, hi_part_len, b1, k) > 0) {
                    uint64_t* hi_diff = arena.allocate(hi_part_len + 1);
                    size_t hd_len = sub_limbs_diff_raw(hi_diff, a_hi + k, hi_part_len, b1, k);
                    add_to_raw(r_hat + k, k + 2, hi_diff, hd_len);
                }
            }
            r_hat_len = 2 * k + 2;
            while (r_hat_len > 0 && r_hat[r_hat_len - 1] == 0) --r_hat_len;
        } else {
            bz_div2by1_raw(q_hat, &q_hat_len, r_hat, &r_hat_len, a_hi, a_hi_len, b1, k, arena);
        }
    }

    // D = q_hat * b0
    uint64_t* d = arena.allocate(2 * k + 1);
    BigIntStorage::zero_limbs(d, 2 * k + 1);
    size_t d_len = 0;
    if (q_hat_len > 0 && b0_len > 0) {
        mul_slice_helper(d, q_hat, q_hat_len, b0, b0_len, arena);
        d_len = q_hat_len + b0_len;
        while (d_len > 0 && d[d_len - 1] == 0) --d_len;
    }

    // r_prime = r_hat * beta^k + a_lo
    uint64_t* r_prime = arena.allocate(3 * k + 4);
    BigIntStorage::zero_limbs(r_prime, 3 * k + 4);
    if (a_lo_len > 0) {
        BigIntStorage::copy_limbs(r_prime, a_lo, a_lo_len);
    }
    if (r_hat_len > 0) {
        BigIntStorage::copy_limbs(r_prime + k, r_hat, r_hat_len);
    }
    size_t r_prime_len = k + r_hat_len;
    while (r_prime_len > 0 && r_prime[r_prime_len - 1] == 0) --r_prime_len;

    // while (r_prime < d)
    while (compare_unsigned(r_prime, r_prime_len, d, d_len) < 0) {
        // --q_hat
        uint8_t borrow = 1;
        for (size_t i = 0; i < k && borrow != 0; ++i) {
            borrow = sbb64(borrow, q_hat[i], 0, &q_hat[i]);
        }
        while (q_hat_len > 0 && q_hat[q_hat_len - 1] == 0) --q_hat_len;

        // r_prime += b
        add_to_raw(r_prime, 3 * k + 4, b, 2 * k);
        r_prime_len = 3 * k + 4;
        while (r_prime_len > 0 && r_prime[r_prime_len - 1] == 0) --r_prime_len;
    }

    // r = r_prime - d
    if (r) {
        BigIntStorage::zero_limbs(r, 2 * k);
        size_t rl = sub_limbs_diff_raw(r, r_prime, r_prime_len, d, d_len);
        if (r_len) *r_len = rl;
    } else if (r_len) {
        *r_len = 0;
    }

    // q = q_hat
    if (q) {
        BigIntStorage::zero_limbs(q, k);
        if (q_hat_len > 0) {
            BigIntStorage::copy_limbs(q, q_hat, q_hat_len);
        }
        if (q_len) *q_len = q_hat_len;
    } else if (q_len) {
        *q_len = 0;
    }
}

inline void BigIntDivision::bz_div2by1_raw(
    uint64_t* NUMERIC_RESTRICT q, size_t* q_len,
    uint64_t* NUMERIC_RESTRICT r, size_t* r_len,
    const uint64_t* a, size_t a_len,
    const uint64_t* b, size_t n,
    ScratchArena& arena)
{
    if (a_len == 0) {
        if (q) BigIntStorage::zero_limbs(q, n);
        if (q_len) *q_len = 0;
        if (r) BigIntStorage::zero_limbs(r, n);
        if (r_len) *r_len = 0;
        return;
    }

    if (n < BZ_THRESHOLD || (n % 2 != 0)) {
        // Base case: Knuth Algorithm D
        size_t total_scratch = (n + 1) + (a_len + 2);
        uint64_t stack_scratch[512];
        uint64_t* scratch_ptr = stack_scratch;
        if (total_scratch > 512) {
            scratch_ptr = arena.allocate(total_scratch);
        }
        if (q) BigIntStorage::zero_limbs(q, n);
        if (r) BigIntStorage::zero_limbs(r, n);
        div_knuth_raw(q, q_len, r, r_len, a, a_len, b, n, scratch_ptr);
        return;
    }

    size_t k = n / 2;
    const uint64_t* a_hi = (a_len > k) ? (a + k) : nullptr;
    size_t a_hi_len = (a_len > k) ? (a_len - k) : 0;
    while (a_hi_len > 0 && a_hi[a_hi_len - 1] == 0) --a_hi_len;

    const uint64_t* a_lo = a;
    size_t a_lo_len = (a_len < k) ? a_len : k;
    while (a_lo_len > 0 && a_lo[a_lo_len - 1] == 0) --a_lo_len;

    ScratchArena::Scope scope(arena);

    uint64_t* q1 = arena.allocate(k + 1);
    uint64_t* r1 = arena.allocate(2 * k + 1);
    size_t q1_len = 0, r1_len = 0;

    bz_div3by2_raw(q1, &q1_len, r1, &r1_len, a_hi, a_hi_len, b, k, arena);

    // a_step2 = r1 * beta^k + a_lo
    uint64_t* a_step2 = arena.allocate(3 * k + 1);
    BigIntStorage::zero_limbs(a_step2, 3 * k + 1);
    if (a_lo_len > 0) {
        BigIntStorage::copy_limbs(a_step2, a_lo, a_lo_len);
    }
    if (r1_len > 0) {
        BigIntStorage::copy_limbs(a_step2 + k, r1, r1_len);
    }
    size_t a_step2_len = k + r1_len;
    while (a_step2_len > 0 && a_step2[a_step2_len - 1] == 0) --a_step2_len;

    uint64_t* q0 = arena.allocate(k + 1);
    uint64_t* r0 = arena.allocate(2 * k + 1);
    size_t q0_len = 0, r0_len = 0;

    bz_div3by2_raw(q0, &q0_len, r0, &r0_len, a_step2, a_step2_len, b, k, arena);

    // q = q1 * beta^k + q0
    if (q) {
        BigIntStorage::zero_limbs(q, 2 * k);
        if (q0_len > 0) {
            BigIntStorage::copy_limbs(q, q0, (std::min)(q0_len, k));
        }
        if (q1_len > 0) {
            BigIntStorage::copy_limbs(q + k, q1, (std::min)(q1_len, k));
        }
        size_t ql = 2 * k;
        while (ql > 0 && q[ql - 1] == 0) --ql;
        if (q_len) *q_len = ql;
    } else if (q_len) {
        *q_len = 0;
    }

    // r = r0
    if (r) {
        BigIntStorage::zero_limbs(r, 2 * k);
        if (r0_len > 0) {
            BigIntStorage::copy_limbs(r, r0, (std::min)(r0_len, 2 * k));
        }
        if (r_len) *r_len = (std::min)(r0_len, 2 * k);
    } else if (r_len) {
        *r_len = 0;
    }
}

} // namespace detail
} // namespace numeric

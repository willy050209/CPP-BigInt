#pragma once

// Multiplication.hpp
// High-performance arbitrary-precision multiplication algorithms for CPP-BigInt:
// Schoolbook, Karatsuba, Toom-Cook 3-way, and ScratchArena allocator.
// Part of the numeric::detail modular core.

#include "Bitwise.hpp"
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 乘法運算核心類別，繼承自 BigIntBitwise。
/// 提供 Schoolbook、Karatsuba、Toom-3 以及 ScratchArena 高效無鎖記憶體池。
/// </summary>
struct BigIntMultiplication : public BigIntBitwise {
    static constexpr size_t KARATSUBA_THRESHOLD = 16;
    static constexpr size_t TOOM3_THRESHOLD = 2048;

    /// <summary>
    /// 原生 Schoolbook 乘法：out = a * b。out 長度至少為 a_len + b_len。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_schoolbook_raw(uint64_t* NUMERIC_RESTRICT out,
                                                        const uint64_t* a, size_t a_len,
                                                        const uint64_t* b, size_t b_len) noexcept {
        BigIntStorage::zero_limbs(out, a_len + b_len);
        for (size_t i = 0; i < a_len; ++i) {
            if (a[i] == 0) continue;
            uint64_t carry = 0;
            for (size_t j = 0; j < b_len; ++j) {
                uint64_t hi = 0;
                uint64_t lo = mul64_wide(a[i], b[j], hi);
                uint64_t cur = out[i + j];
                uint64_t sum = cur + lo;
                uint64_t c1 = (sum < cur) ? 1 : 0;
                sum += carry;
                uint64_t c2 = (sum < carry) ? 1 : 0;
                out[i + j] = sum;
                carry = hi + c1 + c2;
            }
            out[i + b_len] += carry;
        }
    }

    /// <summary>
    /// 原生原地加法：dst += src。回傳溢出進位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 uint64_t add_to_raw(uint64_t* dst, size_t dst_len, const uint64_t* src, size_t src_len) noexcept {
        uint8_t carry = 0;
        size_t min_len = (dst_len < src_len) ? dst_len : src_len;
        for (size_t i = 0; i < min_len; ++i) {
            carry = adc64(carry, dst[i], src[i], &dst[i]);
        }
        for (size_t i = min_len; i < dst_len && carry != 0; ++i) {
            carry = adc64(carry, dst[i], 0, &dst[i]);
        }
        return carry;
    }

    /// <summary>
    /// 原生原地減法：dst -= src。回傳借位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 uint64_t sub_from_raw(uint64_t* dst, size_t dst_len, const uint64_t* src, size_t src_len) noexcept {
        uint8_t borrow = 0;
        size_t min_len = (dst_len < src_len) ? dst_len : src_len;
        for (size_t i = 0; i < min_len; ++i) {
            borrow = sbb64(borrow, dst[i], src[i], &dst[i]);
        }
        for (size_t i = min_len; i < dst_len && borrow != 0; ++i) {
            borrow = sbb64(borrow, dst[i], 0, &dst[i]);
        }
        return borrow;
    }

    /// <summary>
    /// 原生相減差值絕對值：out = a - b (要求 a >= b)。回傳正規化長度。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 size_t sub_limbs_diff_raw(uint64_t* out, const uint64_t* a, size_t a_len, const uint64_t* b, size_t b_len) noexcept {
        uint8_t borrow = 0;
        for (size_t i = 0; i < a_len; ++i) {
            uint64_t bv = (i < b_len) ? b[i] : 0;
            borrow = sbb64(borrow, a[i], bv, &out[i]);
        }
        size_t len = a_len;
        while (len > 0 && out[len - 1] == 0) {
            --len;
        }
        return len;
    }

    /// <summary>
    /// 線程局部暫存記憶體池（Bump Allocator Scratch Arena）。
    /// 提供 Karatsuba 乘法與長整數運算無鎖、0 堆積重配置之 scratch 空間。
    /// 支援 RAII Scope 機制，遞迴或巢狀調用時自動安全回滾偏移指標。
    /// </summary>
    class ScratchArena {
    public:
        static ScratchArena& instance() noexcept {
            static thread_local ScratchArena arena;
            return arena;
        }

        class Scope {
        public:
            explicit NUMERIC_CONSTEXPR_20 Scope(ScratchArena& arena, bool active = true) noexcept
                : m_arena(active ? &arena : nullptr),
                  m_saved_offset(active ? arena.m_offset : 0) {}

            NUMERIC_CONSTEXPR_20 ~Scope() noexcept {
                if (m_arena) {
                    m_arena->m_offset = m_saved_offset;
                }
            }

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
            Scope(Scope&&) = delete;
            Scope& operator=(Scope&&) = delete;

        private:
            ScratchArena* m_arena;
            size_t m_saved_offset;
        };

        uint64_t* allocate(size_t count) {
            if (m_offset + count > m_buffer.size()) {
                size_t new_cap = (std::max)(m_buffer.size() * 2, m_offset + count);
                new_cap = (std::max)(new_cap, static_cast<size_t>(4096));
                m_buffer.resize(new_cap);
            }
            uint64_t* ptr = m_buffer.data() + m_offset;
            m_offset += count;
            return ptr;
        }

    private:
        ScratchArena() = default;
        std::vector<uint64_t> m_buffer;
        size_t m_offset = 0;
    };

    /// <summary>
    /// 減法變體 Scratchpad Karatsuba 核心演算法：out = a * b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_karatsuba_raw(uint64_t* NUMERIC_RESTRICT out,
                                                       const uint64_t* a, size_t a_len,
                                                       const uint64_t* b, size_t b_len,
                                                       uint64_t* scratch) noexcept {
        if (a_len < KARATSUBA_THRESHOLD || b_len < KARATSUBA_THRESHOLD) {
            mul_schoolbook_raw(out, a, a_len, b, b_len);
            return;
        }

        size_t n = (a_len > b_len) ? a_len : b_len;
        size_t m = (n + 1) / 2;

        const uint64_t* a0 = a;
        size_t a0_len = (a_len < m) ? a_len : m;
        while (a0_len > 0 && a0[a0_len - 1] == 0) --a0_len;

        const uint64_t* a1 = a + m;
        size_t a1_len = (a_len > m) ? (a_len - m) : 0;
        while (a1_len > 0 && a1[a1_len - 1] == 0) --a1_len;

        const uint64_t* b0 = b;
        size_t b0_len = (b_len < m) ? b_len : m;
        while (b0_len > 0 && b0[b0_len - 1] == 0) --b0_len;

        const uint64_t* b1 = b + m;
        size_t b1_len = (b_len > m) ? (b_len - m) : 0;
        while (b1_len > 0 && b1[b1_len - 1] == 0) --b1_len;

        // Scratchpad 分配：
        // da: m
        // db: m
        // p:  2*m
        // t:  2*m + 2
        // next_scratch: scratch + 6*m + 2
        uint64_t* da = scratch;
        uint64_t* db = da + m;
        uint64_t* p = db + m;
        uint64_t* t = p + (2 * m);
        uint64_t* next_scratch = t + (2 * m + 2);

        // 1. z0 = a0 * b0 寫入 out[0 .. 2*m - 1]
        BigIntStorage::zero_limbs(out, a_len + b_len);
        size_t z0_len = 0;
        if (a0_len > 0 && b0_len > 0) {
            z0_len = a0_len + b0_len;
            mul_karatsuba_raw(out, a0, a0_len, b0, b0_len, next_scratch);
        }

        // 2. z2 = a1 * b1 寫入 out[2*m .. a_len + b_len - 1]
        size_t z2_len = 0;
        if (a1_len > 0 && b1_len > 0) {
            z2_len = a1_len + b1_len;
            mul_karatsuba_raw(out + 2 * m, a1, a1_len, b1, b1_len, next_scratch);
        }

        // 3. 計算 Da = |a0 - a1|, Db = |b0 - b1|
        int cmp_a = compare_unsigned(a0, a0_len, a1, a1_len);
        int8_t s_a = 0;
        size_t da_len = 0;
        if (cmp_a > 0) {
            s_a = 1;
            da_len = sub_limbs_diff_raw(da, a0, a0_len, a1, a1_len);
        } else if (cmp_a < 0) {
            s_a = -1;
            da_len = sub_limbs_diff_raw(da, a1, a1_len, a0, a0_len);
        }

        int cmp_b = compare_unsigned(b0, b0_len, b1, b1_len);
        int8_t s_b = 0;
        size_t db_len = 0;
        if (cmp_b > 0) {
            s_b = 1;
            db_len = sub_limbs_diff_raw(db, b0, b0_len, b1, b1_len);
        } else if (cmp_b < 0) {
            s_b = -1;
            db_len = sub_limbs_diff_raw(db, b1, b1_len, b0, b0_len);
        }

        int8_t s_p = static_cast<int8_t>(s_a * s_b);
        size_t p_len = 0;
        if (s_p != 0 && da_len > 0 && db_len > 0) {
            p_len = da_len + db_len;
            mul_karatsuba_raw(p, da, da_len, db, db_len, next_scratch);
        } else {
            s_p = 0;
        }

        // 4. 在 t 緩衝區中合成 z1 = z0 + z2 - s_p * p
        if (z0_len > 2 * m) z0_len = 2 * m;
        if (z0_len > 0) {
            BigIntStorage::copy_limbs(t, out, z0_len);
        }
        BigIntStorage::zero_limbs(t + z0_len, (2 * m + 2) - z0_len);

        // 累加 z2
        if (z2_len > 0) {
            add_to_raw(t, 2 * m + 2, out + 2 * m, z2_len);
        }

        // 減法變體符號修正：
        if (s_p > 0 && p_len > 0) {
            sub_from_raw(t, 2 * m + 2, p, p_len);
        } else if (s_p < 0 && p_len > 0) {
            add_to_raw(t, 2 * m + 2, p, p_len);
        }

        // 5. 將 z1 累加至 out + m
        add_to_raw(out + m, (a_len + b_len) - m, t, 2 * m + 2);
    }

    /// <summary>
    /// 精確除以 3：out = out / 3 (前置合約：out 能被 3 整除)。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_exact_3_raw(uint64_t* out, size_t len) noexcept {
        uint64_t rem = 0;
        for (size_t i = len; i > 0; --i) {
            uint64_t next_rem = 0;
            out[i - 1] = div128_64(rem, out[i - 1], 3, next_rem);
            rem = next_rem;
        }
    }

    /// <summary>
    /// 乘法內部遞迴派發：依據運算元規模派發至 Schoolbook, Karatsuba 或 Toom-3。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_dispatch_raw(
        uint64_t* NUMERIC_RESTRICT out,
        const uint64_t* a, size_t a_len,
        const uint64_t* b, size_t b_len,
        uint64_t* scratch) noexcept
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
            mul_karatsuba_raw(out, a, a_len, b, b_len, scratch);
        } else {
            mul_toom3_raw(out, a, a_len, b, b_len, scratch);
        }
    }

    /// <summary>
    /// 生產級 Toom-Cook 3 (Toom-3) 乘法核心演算法：out = a * b。
    /// 漸近時間複雜度 O(N^(log3 5)) ≈ O(N^1.465)，採用 5 點插值法 (0, 1, -1, 2, inf)。
    /// 搭配 ScratchArena 達成全程 0 堆積記憶體配置。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_toom3_raw(
        uint64_t* NUMERIC_RESTRICT out,
        const uint64_t* a, size_t a_len,
        const uint64_t* b, size_t b_len,
        uint64_t* scratch) noexcept
    {
        size_t n = (a_len > b_len) ? a_len : b_len;
        size_t m = (n + 2) / 3;

        const uint64_t* a0 = a;
        size_t a0_len = (a_len < m) ? a_len : m;
        while (a0_len > 0 && a0[a0_len - 1] == 0) --a0_len;

        const uint64_t* a1 = (a_len > m) ? (a + m) : nullptr;
        size_t a1_len = (a_len > m) ? ((a_len < 2 * m) ? (a_len - m) : m) : 0;
        while (a1_len > 0 && a1[a1_len - 1] == 0) --a1_len;

        const uint64_t* a2 = (a_len > 2 * m) ? (a + 2 * m) : nullptr;
        size_t a2_len = (a_len > 2 * m) ? (a_len - 2 * m) : 0;
        while (a2_len > 0 && a2[a2_len - 1] == 0) --a2_len;

        const uint64_t* b0 = b;
        size_t b0_len = (b_len < m) ? b_len : m;
        while (b0_len > 0 && b0[b0_len - 1] == 0) --b0_len;

        const uint64_t* b1 = (b_len > m) ? (b + m) : nullptr;
        size_t b1_len = (b_len > m) ? ((b_len < 2 * m) ? (b_len - m) : m) : 0;
        while (b1_len > 0 && b1[b1_len - 1] == 0) --b1_len;

        const uint64_t* b2 = (b_len > 2 * m) ? (b + 2 * m) : nullptr;
        size_t b2_len = (b_len > 2 * m) ? (b_len - 2 * m) : 0;
        while (b2_len > 0 && b2[b2_len - 1] == 0) --b2_len;

        size_t ev_sz = m + 2;
        size_t pr_sz = 2 * m + 4;

        uint64_t* p1   = scratch;
        uint64_t* q1   = p1 + ev_sz;
        uint64_t* p_m1 = q1 + ev_sz;
        uint64_t* q_m1 = p_m1 + ev_sz;
        uint64_t* p2   = q_m1 + ev_sz;
        uint64_t* q2   = p2 + ev_sz;

        uint64_t* v1   = q2 + ev_sz;
        uint64_t* v_m1 = v1 + pr_sz;
        uint64_t* v2   = v_m1 + pr_sz;
        uint64_t* next_scratch = v2 + pr_sz;

        BigIntStorage::zero_limbs(scratch, (v2 + pr_sz) - scratch);
        BigIntStorage::zero_limbs(out, a_len + b_len);

        if (a0_len > 0 && b0_len > 0) {
            mul_dispatch_raw(out, a0, a0_len, b0, b0_len, next_scratch);
        }

        uint64_t* v_inf = out + 4 * m;
        if (a2_len > 0 && b2_len > 0) {
            mul_dispatch_raw(v_inf, a2, a2_len, b2, b2_len, next_scratch);
        }

        BigIntStorage::copy_limbs(p1, a0, a0_len);
        if (a2_len > 0) add_to_raw(p1, ev_sz, a2, a2_len);
        BigIntStorage::copy_limbs(p_m1, p1, ev_sz);
        if (a1_len > 0) add_to_raw(p1, ev_sz, a1, a1_len);

        int8_t s_a = 1;
        size_t a0_a2_len = ev_sz;
        while (a0_a2_len > 0 && p_m1[a0_a2_len - 1] == 0) --a0_a2_len;
        int cmp_a = compare_unsigned(p_m1, a0_a2_len, a1, a1_len);
        if (cmp_a >= 0) {
            if (a1_len > 0) sub_from_raw(p_m1, ev_sz, a1, a1_len);
            s_a = 1;
        } else {
            uint64_t tmp[256];
            uint64_t* t_ptr = (ev_sz <= 256) ? tmp : next_scratch;
            BigIntStorage::copy_limbs(t_ptr, a1, a1_len);
            BigIntStorage::zero_limbs(t_ptr + a1_len, ev_sz - a1_len);
            sub_from_raw(t_ptr, ev_sz, p_m1, a0_a2_len);
            BigIntStorage::copy_limbs(p_m1, t_ptr, ev_sz);
            s_a = -1;
        }

        if (a2_len > 0) {
            add_to_raw(p2, ev_sz, a2, a2_len);
            add_to_raw(p2, ev_sz, a2, a2_len);
        }
        if (a1_len > 0) add_to_raw(p2, ev_sz, a1, a1_len);
        uint64_t carry = 0;
        for (size_t i = 0; i < ev_sz; ++i) {
            uint64_t cur = p2[i];
            p2[i] = (cur << 1) | carry;
            carry = cur >> 63;
        }
        if (a0_len > 0) add_to_raw(p2, ev_sz, a0, a0_len);

        BigIntStorage::copy_limbs(q1, b0, b0_len);
        if (b2_len > 0) add_to_raw(q1, ev_sz, b2, b2_len);
        BigIntStorage::copy_limbs(q_m1, q1, ev_sz);
        if (b1_len > 0) add_to_raw(q1, ev_sz, b1, b1_len);

        int8_t s_b = 1;
        size_t b0_b2_len = ev_sz;
        while (b0_b2_len > 0 && q_m1[b0_b2_len - 1] == 0) --b0_b2_len;
        int cmp_b = compare_unsigned(q_m1, b0_b2_len, b1, b1_len);
        if (cmp_b >= 0) {
            if (b1_len > 0) sub_from_raw(q_m1, ev_sz, b1, b1_len);
            s_b = 1;
        } else {
            uint64_t tmp[256];
            uint64_t* t_ptr = (ev_sz <= 256) ? tmp : next_scratch;
            BigIntStorage::copy_limbs(t_ptr, b1, b1_len);
            BigIntStorage::zero_limbs(t_ptr + b1_len, ev_sz - b1_len);
            sub_from_raw(t_ptr, ev_sz, q_m1, b0_b2_len);
            BigIntStorage::copy_limbs(q_m1, t_ptr, ev_sz);
            s_b = -1;
        }

        if (b2_len > 0) {
            add_to_raw(q2, ev_sz, b2, b2_len);
            add_to_raw(q2, ev_sz, b2, b2_len);
        }
        if (b1_len > 0) add_to_raw(q2, ev_sz, b1, b1_len);
        carry = 0;
        for (size_t i = 0; i < ev_sz; ++i) {
            uint64_t cur = q2[i];
            q2[i] = (cur << 1) | carry;
            carry = cur >> 63;
        }
        if (b0_len > 0) add_to_raw(q2, ev_sz, b0, b0_len);

        size_t p1_len = ev_sz; while (p1_len > 0 && p1[p1_len - 1] == 0) --p1_len;
        size_t q1_len = ev_sz; while (q1_len > 0 && q1[q1_len - 1] == 0) --q1_len;
        if (p1_len > 0 && q1_len > 0) {
            mul_dispatch_raw(v1, p1, p1_len, q1, q1_len, next_scratch);
        }

        size_t pm1_len = ev_sz; while (pm1_len > 0 && p_m1[pm1_len - 1] == 0) --pm1_len;
        size_t qm1_len = ev_sz; while (qm1_len > 0 && q_m1[qm1_len - 1] == 0) --qm1_len;
        if (pm1_len > 0 && qm1_len > 0) {
            mul_dispatch_raw(v_m1, p_m1, pm1_len, q_m1, qm1_len, next_scratch);
        }
        int8_t s_vm1 = static_cast<int8_t>(s_a * s_b);

        size_t p2_len = ev_sz; while (p2_len > 0 && p2[p2_len - 1] == 0) --p2_len;
        size_t q2_len = ev_sz; while (q2_len > 0 && q2[q2_len - 1] == 0) --q2_len;
        if (p2_len > 0 && q2_len > 0) {
            mul_dispatch_raw(v2, p2, p2_len, q2, q2_len, next_scratch);
        }

        BigIntStorage st_v0, st_vinf, st_v1, st_vm1, st_v2;
        st_v0.resize(2 * m);
        BigIntStorage::copy_limbs(st_v0.data(), out, 2 * m);
        st_v0.m_sign = 1;
        st_v0.normalize();

        size_t vinf_sz = (a_len + b_len > 4 * m) ? (a_len + b_len - 4 * m) : 0;
        st_vinf.resize(vinf_sz);
        if (vinf_sz > 0) {
            BigIntStorage::copy_limbs(st_vinf.data(), v_inf, vinf_sz);
        }
        st_vinf.m_sign = 1;
        st_vinf.normalize();

        st_v1.resize(pr_sz);
        BigIntStorage::copy_limbs(st_v1.data(), v1, pr_sz);
        st_v1.m_sign = 1;
        st_v1.normalize();

        st_vm1.resize(pr_sz);
        BigIntStorage::copy_limbs(st_vm1.data(), v_m1, pr_sz);
        st_vm1.m_sign = s_vm1;
        st_vm1.normalize();

        st_v2.resize(pr_sz);
        BigIntStorage::copy_limbs(st_v2.data(), v2, pr_sz);
        st_v2.m_sign = 1;
        st_v2.normalize();

        BigIntStorage sum_v1_vm1, t1;
        add_signed(sum_v1_vm1, st_v1, st_vm1);
        shift_right(t1, sum_v1_vm1, 1);

        BigIntStorage diff_v1_vm1, t2;
        sub_signed(diff_v1_vm1, st_v1, st_vm1);
        shift_right(t2, diff_v1_vm1, 1);

        BigIntStorage c2_step1, c2;
        sub_signed(c2_step1, t1, st_v0);
        sub_signed(c2, c2_step1, st_vinf);

        BigIntStorage v2_minus_v0, v2_step;
        sub_signed(v2_minus_v0, st_v2, st_v0);
        shift_right(v2_step, v2_minus_v0, 1);

        BigIntStorage v2_step2;
        sub_signed(v2_step2, v2_step, t2);

        BigIntStorage eight_v_inf, v2_step3;
        shift_left(eight_v_inf, st_vinf, 3);
        sub_signed(v2_step3, v2_step2, eight_v_inf);

        BigIntStorage two_c2, three_c3;
        shift_left(two_c2, c2, 1);
        sub_signed(three_c3, v2_step3, two_c2);

        BigIntStorage c3 = three_c3;
        div_exact_3_raw(c3.data(), c3.m_size);
        c3.normalize();

        BigIntStorage c1;
        sub_signed(c1, t2, c3);

        if (c1.m_size > 0) add_to_raw(out + m, (a_len + b_len) - m, c1.data(), c1.m_size);
        if (c2.m_size > 0) add_to_raw(out + 2 * m, (a_len + b_len) - 2 * m, c2.data(), c2.m_size);
        if (c3.m_size > 0) add_to_raw(out + 3 * m, (a_len + b_len) - 3 * m, c3.data(), c3.m_size);
    }

    /// <summary>
    /// 傳統 Schoolbook 長整數乘法演算法。
    /// </summary>
    /// <param name="res">輸出乘積儲存物件</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_schoolbook(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_size == 0 || b.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            tmp.resize(a.m_size + b.m_size, 0);
            mul_schoolbook_raw(tmp.data(), a.data(), a.m_size, b.data(), b.m_size);
            tmp.m_sign = 1;
            tmp.normalize();
            res = std::move(tmp);
        } else {
            res.resize(a.m_size + b.m_size, 0);
            mul_schoolbook_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size);
            res.m_sign = 1;
            res.normalize();
        }
    }

    /// <summary>
    /// 單肢段非平衡乘法：res = a * b_val（含自我別名 &res == &a 防護）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_single_limb(BigIntStorage& res, const BigIntStorage& a, uint64_t b_val, int8_t sign) {
        if (b_val == 0 || a.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (b_val == 1) {
            if (&res != &a) {
                res = a;
            }
            res.m_sign = sign;
            return;
        }

        size_t n = a.m_size;
        // 自我別名防護：先一次性確認容量足夠，絕不允許在掃描迴圈中途擴容導致指針懸置
        if (res.m_capacity < n + 1) {
            res.reserve(n + 1);
        }

        uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t high = 0;
            uint64_t low = mul64_wide(a.data()[i], b_val, high);
            low += carry;
            if (low < carry) ++high;
            res.data()[i] = low;
            carry = high;
        }
        if (carry > 0) {
            res.data()[n] = carry;
            res.m_size = n + 1;
        } else {
            res.m_size = n;
        }
        res.m_sign = sign;
    }

    /// <summary>
    /// Karatsuba 快速乘法演算法（減法變體 + Scratchpad 記憶體規劃）。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_karatsuba(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
        if (n < KARATSUBA_THRESHOLD || a.m_size == 0 || b.m_size == 0) {
            mul_schoolbook(res, a, b);
            return;
        }

        // 自我別名防護
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            mul_karatsuba(tmp, a, b);
            res = std::move(tmp);
            return;
        }

        size_t total_len = a.m_size + b.m_size;
        res.resize(total_len, 0);

        // Scratchpad 預估大小：8 * n + 128 limbs
        size_t scratch_size = 8 * n + 128;
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            if (scratch_size <= 1024) {
                uint64_t stack_scratch[1024];
                mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
            } else {
                std::vector<uint64_t> heap_scratch(scratch_size);
                mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, heap_scratch.data());
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
#endif
        if (scratch_size <= 1024) {
            uint64_t stack_scratch[1024];
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
        } else {
            auto& arena = ScratchArena::instance();
            ScratchArena::Scope scope(arena);
            uint64_t* scratch = arena.allocate(scratch_size);
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, scratch);
        }

        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// Toom-3 (Toom-Cook 3-way) 快速乘法演算法（ScratchArena 記憶體池加速）。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_toom3(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
        if (n < TOOM3_THRESHOLD || a.m_size == 0 || b.m_size == 0) {
            mul_karatsuba(res, a, b);
            return;
        }

        // 自我別名防護
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            mul_toom3(tmp, a, b);
            res = std::move(tmp);
            return;
        }

        size_t total_len = a.m_size + b.m_size;
        res.resize(total_len, 0);

        // Scratchpad 預估大小：48 * n + 2048 limbs
        size_t scratch_size = 48 * n + 2048;
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            if (scratch_size <= 2048) {
                uint64_t stack_scratch[2048];
                mul_toom3_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
            } else {
                std::vector<uint64_t> heap_scratch(scratch_size);
                mul_toom3_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, heap_scratch.data());
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
#endif
        if (scratch_size <= 2048) {
            uint64_t stack_scratch[2048];
            mul_toom3_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
        } else {
            auto& arena = ScratchArena::instance();
            ScratchArena::Scope scope(arena);
            uint64_t* scratch = arena.allocate(scratch_size);
            mul_toom3_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, scratch);
        }

        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// 內部乘法派發函式，依據位數自動切換 Schoolbook, Karatsuba 與 Toom-3。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_core(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
        if (a.m_size < KARATSUBA_THRESHOLD || b.m_size < KARATSUBA_THRESHOLD ||
            a.m_size >= 2 * b.m_size || b.m_size >= 2 * a.m_size) {
            mul_schoolbook(res, a, b);
        } else if (n < TOOM3_THRESHOLD) {
            mul_karatsuba(res, a, b);
        } else {
            mul_toom3(res, a, b);
        }
    }

    /// <summary>
    /// 帶符號乘法：res = a * b。
    /// 包含 M0 (零值快路徑與非平衡乘法)、M1 (單肢段原生直通)、M2/M3 (Schoolbook/Karatsuba)。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        // M0: 零值快路徑
        if (a.m_sign == 0 || b.m_sign == 0 || a.m_size == 0 || b.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        int8_t res_sign = static_cast<int8_t>(a.m_sign * b.m_sign);

        // M1: 1-Limb 原生直通
        if (a.m_size == 1 && b.m_size == 1) {
            uint64_t hi = 0;
            uint64_t lo = mul64_wide(a.data()[0], b.data()[0], hi);
            res.reset_heap();
            res.data()[0] = lo;
            if (hi > 0) {
                res.data()[1] = hi;
                res.m_size = 2;
            } else {
                res.m_size = 1;
            }
            res.m_sign = res_sign;
            return;
        }

        // 單肢段非平衡乘法
        if (a.m_size == 1) {
            mul_single_limb(res, b, a.data()[0], res_sign);
            return;
        }
        if (b.m_size == 1) {
            mul_single_limb(res, a, b.data()[0], res_sign);
            return;
        }

        // M2 & M3 派發
        mul_core(res, a, b);
        res.m_sign = res_sign;
    }
};

} // namespace detail
} // namespace numeric

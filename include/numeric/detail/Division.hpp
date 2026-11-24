// filepath: include/numeric/detail/Division.hpp
/**
 * @file Division.hpp
 * @brief Arbitrary-precision integer division and modulo algorithms for CPP-BigInt.
 *        Implements fast single-limb division, Knuth Algorithm D, and zero-heap Burnikel-Ziegler
 *        3/2 and 2/1 divide-and-conquer division over bounds-safe std::span slices.
 *        任意精度整數除法與取模演算法模組。
 *        實作單肢段快除、Knuth Algorithm D 與基於 std::span 之零堆積 Burnikel-Ziegler 分治除法。
 */
#pragma once

#include "Multiplication.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// Arbitrary-precision division algorithms inheriting BigIntMultiplication.
/// BigInt 除法運算核心類別，繼承自 BigIntMultiplication。
/// </summary>
struct BigIntDivision : public BigIntMultiplication {
    static constexpr std::size_t BZ_THRESHOLD{32U}; // 2,048-bit Burnikel-Ziegler threshold / 2048 位元分治除法門檻

    /// <summary>
    /// Slice multiplication helper for Burnikel-Ziegler: out = a * b using arena scratch.
    /// 分治除法內部乘法輔助函式：依運算元規模分派並從 arena 配置暫存空間。
    /// </summary>
    static auto mul_slice_helper(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b,
        ScratchArena& arena) noexcept -> void
    {
        if (a.empty() || b.empty()) {
            BigIntStorage::zero_limbs(out.subspan(0U, a.size() + b.size()));
            return;
        }
        const std::size_t n{std::max(a.size(), b.size())};
        const std::size_t scratch_sz{(n >= TOOM3_THRESHOLD) ? (48U * n + 2048U) : (8U * n + 256U)};
        const std::span<std::uint64_t> scratch{arena.allocate(scratch_sz)};
        mul_dispatch_raw(out, a, b, scratch);
    }

    /// <summary>
    /// Knuth Algorithm D raw long division over std::span buffers.
    /// Skips quotient or remainder computation cleanly when the corresponding output span is empty.
    /// Knuth Algorithm D 原地長除法底層 Raw 介面，當商或餘數 span 為空時自動跳過對應存取。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto div_knuth_raw(
        const std::span<std::uint64_t> q_out, std::size_t* const q_len,
        const std::span<std::uint64_t> r_out, std::size_t* const r_len,
        const std::span<const std::uint64_t> u,
        const std::span<const std::uint64_t> v,
        const std::span<std::uint64_t> scratch) noexcept -> void
    {
        const std::size_t u_len{u.size()};
        const std::size_t v_len{v.size()};
        if (v_len == 0U) {
            return;
        }
        if (u_len == 0U || u_len < v_len) {
            if (q_len != nullptr) *q_len = 0U;
            if (!r_out.empty() && u_len > 0U) {
                BigIntStorage::copy_limbs(r_out, u);
            }
            if (r_len != nullptr) *r_len = u_len;
            return;
        }

        const std::int32_t cmp{compare_unsigned(u, v)};
        if (cmp < 0) {
            if (q_len != nullptr) *q_len = 0U;
            if (!r_out.empty() && u_len > 0U) {
                BigIntStorage::copy_limbs(r_out, u);
            }
            if (r_len != nullptr) *r_len = u_len;
            return;
        }
        if (cmp == 0) {
            if (!q_out.empty()) q_out[0] = 1ULL;
            if (q_len != nullptr) *q_len = 1U;
            if (r_len != nullptr) *r_len = 0U;
            return;
        }

        if (v_len == 1U) {
            const std::uint64_t divisor{v[0]};
            std::uint64_t rem{0};
            if (!q_out.empty()) {
                for (std::size_t i{u_len}; i > 0U; --i) {
                    std::uint64_t next_rem{0};
                    q_out[i - 1U] = div128_64(rem, u[i - 1U], divisor, next_rem);
                    rem = next_rem;
                }
                std::size_t ql{u_len};
                while (ql > 0U && q_out[ql - 1U] == 0ULL) --ql;
                if (q_len != nullptr) *q_len = ql;
            } else {
                for (std::size_t i{u_len}; i > 0U; --i) {
                    std::uint64_t next_rem{0};
                    (void)div128_64(rem, u[i - 1U], divisor, next_rem);
                    rem = next_rem;
                }
                if (q_len != nullptr) *q_len = 0U;
            }
            if (!r_out.empty()) {
                r_out[0] = rem;
                if (r_len != nullptr) *r_len = (rem != 0ULL ? 1U : 0U);
            } else if (r_len != nullptr) {
                *r_len = (rem != 0ULL ? 1U : 0U);
            }
            return;
        }

        const std::size_t n{v_len};
        const std::size_t m{u_len - n};

        const std::int32_t s{clz64(v[n - 1U])};
        const std::span<std::uint64_t> vn{scratch.subspan(0U, n + 1U)};
        const std::span<std::uint64_t> un{scratch.subspan(n + 1U, u_len + 2U)};

        if (s == 0) {
            BigIntStorage::copy_limbs(vn, v);
            BigIntStorage::copy_limbs(un, u);
            un[u_len] = 0ULL;
            un[u_len + 1U] = 0ULL;
        } else {
            const auto shift_l{static_cast<std::uint32_t>(s)};
            std::uint64_t carry{0};
            for (std::size_t i{0}; i < n; ++i) {
                const std::uint64_t cur{v[i]};
                vn[i] = (cur << shift_l) | carry;
                carry = cur >> (64U - shift_l);
            }
            vn[n] = carry;

            carry = 0ULL;
            for (std::size_t i{0}; i < u_len; ++i) {
                const std::uint64_t cur{u[i]};
                un[i] = (cur << shift_l) | carry;
                carry = cur >> (64U - shift_l);
            }
            un[u_len] = carry;
            un[u_len + 1U] = 0ULL;
        }

        const std::uint64_t v_hi{vn[n - 1U]};
        const std::uint64_t v_lo{vn[n - 2U]};

        for (std::size_t k_idx{m + 1U}; k_idx > 0U; --k_idx) {
            const std::size_t j{k_idx - 1U};
            const std::uint64_t u_hi{un[j + n]};
            const std::uint64_t u_mid{un[j + n - 1U]};
            const std::uint64_t u_lo{(j + n >= 2U) ? un[j + n - 2U] : 0ULL};

            std::uint64_t q_hat{0};
            std::uint64_t r_hat{0};

            if (u_hi == v_hi) {
                q_hat = 0xFFFFFFFFFFFFFFFFULL;
                r_hat = u_mid + v_hi;
                if (r_hat >= v_hi) {
                    std::uint64_t p_hi{0};
                    std::uint64_t p_lo{mul64_wide(q_hat, v_lo, p_hi)};
                    if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                        --q_hat;
                        r_hat += v_hi;
                        if (r_hat >= v_hi) {
                            p_lo = mul64_wide(q_hat, v_lo, p_hi);
                            if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                                --q_hat;
                            }
                        }
                    }
                }
            } else {
                q_hat = div128_64(u_hi, u_mid, v_hi, r_hat);
                std::uint64_t p_hi{0};
                std::uint64_t p_lo{mul64_wide(q_hat, v_lo, p_hi)};
                if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                    --q_hat;
                    r_hat += v_hi;
                    if (r_hat >= v_hi) {
                        p_lo = mul64_wide(q_hat, v_lo, p_hi);
                        if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                            --q_hat;
                        }
                    }
                }
            }

            const std::uint64_t borrow{submul_1_raw(un.subspan(j, n), vn.subspan(0U, n), q_hat)};
            const std::uint64_t cur{un[j + n]};
            const std::uint64_t b{(cur < borrow) ? 1ULL : 0ULL};
            un[j + n] = cur - borrow;

            if (b > 0ULL) {
                --q_hat;
                const std::uint64_t add_carry{add_to_raw(un.subspan(j, n), vn.subspan(0U, n))};
                un[j + n] += add_carry;
            }

            if (!q_out.empty()) {
                q_out[j] = q_hat;
            }
        }

        if (!q_out.empty() && q_len != nullptr) {
            std::size_t ql{m + 1U};
            while (ql > 0U && q_out[ql - 1U] == 0ULL) --ql;
            *q_len = ql;
        }

        if (!r_out.empty()) {
            if (s > 0) {
                const auto shift_r{static_cast<std::uint32_t>(s)};
                for (std::size_t i{0}; i < n; ++i) {
                    const std::uint64_t c{un[i]};
                    const std::uint64_t next{(i + 1U <= n) ? un[i + 1U] : 0ULL};
                    r_out[i] = (c >> shift_r) | (next << (64U - shift_r));
                }
            } else {
                BigIntStorage::copy_limbs(r_out, un.subspan(0U, n));
            }
            if (r_len != nullptr) {
                std::size_t rl{n};
                while (rl > 0U && r_out[rl - 1U] == 0ULL) --rl;
                *r_len = rl;
            }
        } else if (r_len != nullptr) {
            *r_len = 0U;
        }
    }

    /// <summary>
    /// High-level Knuth Algorithm D long division into BigIntStorage targets.
    /// Knuth Algorithm D 原地長除法高階包裝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto div_mod_core_knuth(
        BigIntStorage* const q_out,
        BigIntStorage* const r_out,
        const BigIntStorage& u,
        const BigIntStorage& v) -> void
    {
        if (v.m_size == 0U) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0U || u.m_size < v.m_size) {
            if (q_out != nullptr) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out != nullptr) { *r_out = u; r_out->m_sign = (u.m_size > 0U ? 1 : 0); }
            return;
        }
        const std::int32_t cmp{compare_unsigned(u.as_span(), v.as_span())};
        if (cmp < 0) {
            if (q_out != nullptr) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out != nullptr) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out != nullptr) { q_out->set_uint64(1ULL, 1); }
            if (r_out != nullptr) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        const std::size_t n{v.m_size};
        const std::size_t m{u.m_size - n};
        const std::size_t total_scratch{(n + 1U) + (u.m_size + 2U)};

        if (q_out != nullptr && q_out != &u) {
            q_out->resize(m + 1U, 0ULL);
        }
        if (r_out != nullptr && r_out != &u) {
            r_out->resize(n, 0ULL);
        }

        std::size_t ql{0};
        std::size_t rl{0};
        const std::span<std::uint64_t> q_span{(q_out != nullptr) ? q_out->capacity_span() : std::span<std::uint64_t>{}};
        const std::span<std::uint64_t> r_span{(r_out != nullptr) ? r_out->capacity_span() : std::span<std::uint64_t>{}};

        if (std::is_constant_evaluated()) {
            if (total_scratch <= 512U) {
                std::array<std::uint64_t, 512> stack_scratch{};
                div_knuth_raw(q_span, &ql, r_span, &rl, u.as_span(), v.as_span(), stack_scratch);
            } else {
                std::vector<std::uint64_t> heap_scratch(total_scratch, 0ULL);
                div_knuth_raw(q_span, &ql, r_span, &rl, u.as_span(), v.as_span(), heap_scratch);
            }
        } else if (total_scratch <= 512U) {
            std::array<std::uint64_t, 512> stack_scratch;
            div_knuth_raw(q_span, &ql, r_span, &rl, u.as_span(), v.as_span(), stack_scratch);
        } else {
            auto& arena{ScratchArena::instance()};
            const ScratchArena::Scope scope{arena};
            const std::span<std::uint64_t> scratch_span{arena.allocate(total_scratch)};
            div_knuth_raw(q_span, &ql, r_span, &rl, u.as_span(), v.as_span(), scratch_span);
        }

        if (q_out != nullptr) {
            q_out->m_size = static_cast<std::uint32_t>(ql);
            q_out->m_sign = (ql > 0U ? 1 : 0);
            q_out->normalize();
        }
        if (r_out != nullptr) {
            r_out->m_size = static_cast<std::uint32_t>(rl);
            r_out->m_sign = (rl > 0U ? 1 : 0);
            r_out->normalize();
        }
    }

    /// <summary>
    /// Burnikel-Ziegler div3by2 raw over std::span slices.
    /// Burnikel-Ziegler 3/2 分治除法底層 Raw 介面（基於 std::span 零堆積配置）。
    /// </summary>
    static auto bz_div3by2_raw(
        std::span<std::uint64_t> q, std::size_t* q_len,
        std::span<std::uint64_t> r, std::size_t* r_len,
        std::span<const std::uint64_t> a,
        std::span<const std::uint64_t> b, std::size_t k,
        ScratchArena& arena) -> void;

    /// <summary>
    /// Burnikel-Ziegler div2by1 raw over std::span slices.
    /// Burnikel-Ziegler 2/1 分治除法底層 Raw 介面（基於 std::span 零堆積配置）。
    /// </summary>
    static auto bz_div2by1_raw(
        std::span<std::uint64_t> q, std::size_t* q_len,
        std::span<std::uint64_t> r, std::size_t* r_len,
        std::span<const std::uint64_t> a,
        std::span<const std::uint64_t> b, std::size_t n,
        ScratchArena& arena) -> void;

    /// <summary>
    /// High-level Burnikel-Ziegler div3by2 wrapper.
    /// Burnikel-Ziegler div3by2 高階包裝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto bz_div3by2(
        BigIntStorage* const q,
        BigIntStorage* const r,
        const BigIntStorage& a,
        const BigIntStorage& b,
        const std::size_t k) -> void
    {
        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }
        auto& arena{ScratchArena::instance()};
        const ScratchArena::Scope scope{arena};

        const std::span<std::uint64_t> q_buf{arena.allocate(k + 1U)};
        const std::span<std::uint64_t> r_buf{arena.allocate(2U * k + 1U)};
        std::size_t ql{0};
        std::size_t rl{0};

        bz_div3by2_raw(q_buf, &ql, r_buf, &rl, a.as_span(), b.as_span(), k, arena);

        if (q != nullptr) {
            q->resize(ql, 0ULL);
            if (ql > 0U) BigIntStorage::copy_limbs(q->as_span(), q_buf.subspan(0U, ql));
            q->m_size = static_cast<std::uint32_t>(ql);
            q->m_sign = (ql > 0U) ? 1 : 0;
            q->normalize();
        }
        if (r != nullptr) {
            r->resize(rl, 0ULL);
            if (rl > 0U) BigIntStorage::copy_limbs(r->as_span(), r_buf.subspan(0U, rl));
            r->m_size = static_cast<std::uint32_t>(rl);
            r->m_sign = (rl > 0U) ? 1 : 0;
            r->normalize();
        }
    }

    /// <summary>
    /// High-level Burnikel-Ziegler div2by1 wrapper.
    /// Burnikel-Ziegler div2by1 高階包裝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto bz_div2by1(
        BigIntStorage* const q,
        BigIntStorage* const r,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }
        const std::size_t n{b.m_size};
        if (n < BZ_THRESHOLD || (n % 2U != 0U)) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }

        auto& arena{ScratchArena::instance()};
        const ScratchArena::Scope scope{arena};

        const std::span<std::uint64_t> q_buf{arena.allocate(n + 1U)};
        const std::span<std::uint64_t> r_buf{arena.allocate(n + 1U)};
        std::size_t ql{0};
        std::size_t rl{0};

        bz_div2by1_raw(q_buf, &ql, r_buf, &rl, a.as_span(), b.as_span(), n, arena);

        if (q != nullptr) {
            q->resize(ql, 0ULL);
            if (ql > 0U) BigIntStorage::copy_limbs(q->as_span(), q_buf.subspan(0U, ql));
            q->m_size = static_cast<std::uint32_t>(ql);
            q->m_sign = (ql > 0U) ? 1 : 0;
            q->normalize();
        }
        if (r != nullptr) {
            r->resize(rl, 0ULL);
            if (rl > 0U) BigIntStorage::copy_limbs(r->as_span(), r_buf.subspan(0U, rl));
            r->m_size = static_cast<std::uint32_t>(rl);
            r->m_sign = (rl > 0U) ? 1 : 0;
            r->normalize();
        }
    }

    /// <summary>
    /// Primary unsigned long division and modulo dispatcher.
    /// Dispatches to single-limb fast division, Knuth Algorithm D, or Burnikel-Ziegler.
    /// 核心長除法與取模派發入口：整合單 limb 快除、Knuth Algorithm D 與 Burnikel-Ziegler 分治除法。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto div_mod_core(
        BigIntStorage* const q_out,
        BigIntStorage* const r_out,
        const BigIntStorage& u,
        const BigIntStorage& v) -> void
    {
        if (v.m_size == 0U) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0U) {
            if (q_out != nullptr) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out != nullptr) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }
        const std::int32_t cmp{compare_unsigned(u.as_span(), v.as_span())};
        if (cmp < 0) {
            if (q_out != nullptr) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out != nullptr) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out != nullptr) { q_out->set_uint64(1ULL, 1); }
            if (r_out != nullptr) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        if (v.m_size == 1U) {
            const std::uint64_t divisor{v.as_span()[0]};
            if (q_out != nullptr) q_out->resize(u.m_size, 0ULL);
            std::uint64_t rem{0};
            const std::span<const std::uint64_t> u_span{u.as_span()};
            for (std::size_t i{u.m_size}; i > 0U; --i) {
                std::uint64_t next_rem{0};
                const std::uint64_t q_limb{div128_64(rem, u_span[i - 1U], divisor, next_rem)};
                if (q_out != nullptr) (*q_out)[i - 1U] = q_limb;
                rem = next_rem;
            }
            if (q_out != nullptr) {
                q_out->m_sign = 1;
                q_out->normalize();
            }
            if (r_out != nullptr) {
                if (rem != 0ULL) {
                    r_out->set_uint64(rem, 1);
                } else {
                    r_out->m_size = 0;
                    r_out->m_sign = 0;
                }
            }
            return;
        }

        if (std::is_constant_evaluated()) {
            div_mod_core_knuth(q_out, r_out, u, v);
            return;
        }

        const std::size_t m_len{(u.m_size >= v.m_size) ? (u.m_size - v.m_size) : 0U};
        if (v.m_size < BZ_THRESHOLD || m_len < BZ_THRESHOLD) {
            div_mod_core_knuth(q_out, r_out, u, v);
            return;
        }

        const std::size_t s{v.m_size};
        std::size_t m_pow2{2U};
        while (m_pow2 * BZ_THRESHOLD <= s) {
            m_pow2 <<= 1U;
        }
        const std::size_t j{(s + m_pow2 - 1U) / m_pow2};
        const std::size_t n{j * m_pow2};

        const std::size_t n_bits{n * 64U};
        const std::size_t v_bits{(v.m_size - 1U) * 64U + static_cast<std::size_t>(64 - clz64(v.as_span()[v.m_size - 1U]))};
        const std::size_t sigma{(n_bits >= v_bits) ? (n_bits - v_bits) : 0U};

        auto& arena{ScratchArena::instance()};
        const ScratchArena::Scope scope{arena};

        const std::size_t u_len{u.m_size};
        const std::size_t sigma_limbs{sigma / 64U};
        const auto sigma_bits{static_cast<std::uint32_t>(sigma % 64U)};

        const std::span<std::uint64_t> v_shifted{arena.allocate(n + 2U)};
        BigIntStorage::zero_limbs(v_shifted);
        if (sigma > 0U) {
            if (sigma_bits == 0U) {
                BigIntStorage::copy_limbs(v_shifted.subspan(sigma_limbs), v.as_span());
            } else {
                (void)shift_left_bits_raw(v_shifted.subspan(sigma_limbs), v.as_span(), sigma_bits);
            }
        } else {
            BigIntStorage::copy_limbs(v_shifted, v.as_span());
        }

        const std::size_t u_shifted_cap{u_len + sigma_limbs + 4U};
        const std::size_t u_alloc_sz{std::max(u_shifted_cap, n * 4U) + n};
        const std::span<std::uint64_t> u_shifted{arena.allocate(u_alloc_sz)};
        BigIntStorage::zero_limbs(u_shifted);
        std::size_t u_shifted_len{u_len};
        if (sigma > 0U) {
            if (sigma_bits == 0U) {
                BigIntStorage::copy_limbs(u_shifted.subspan(sigma_limbs), u.as_span());
                u_shifted_len = u_len + sigma_limbs;
            } else {
                u_shifted_len = sigma_limbs + shift_left_bits_raw(u_shifted.subspan(sigma_limbs), u.as_span(), sigma_bits);
            }
        } else {
            BigIntStorage::copy_limbs(u_shifted, u.as_span());
        }
        while (u_shifted_len > 0U && u_shifted[u_shifted_len - 1U] == 0ULL) --u_shifted_len;

        const std::size_t u_bits{(u_shifted_len > 0U)
            ? ((u_shifted_len - 1U) * 64U + static_cast<std::size_t>(64 - clz64(u_shifted[u_shifted_len - 1U])))
            : 0U};
        std::size_t t{(u_bits + n_bits) / n_bits};
        if (t < 2U) t = 2U;

        const std::span<std::uint64_t> z{arena.allocate(2U * n + 1U)};
        const std::span<std::uint64_t> q_step{arena.allocate(n + 1U)};
        const std::span<std::uint64_t> r_cur{arena.allocate(n + 1U)};
        const std::span<std::uint64_t> r_next{arena.allocate(n + 1U)};

        BigIntStorage::zero_limbs(r_cur);
        const std::size_t top_start{(t - 1U) * n};
        std::size_t r_cur_len{0};
        if (top_start < u_shifted_len) {
            r_cur_len = std::min(n, u_shifted_len - top_start);
            BigIntStorage::copy_limbs(r_cur, u_shifted.subspan(top_start, r_cur_len));
            while (r_cur_len > 0U && r_cur[r_cur_len - 1U] == 0ULL) --r_cur_len;
        }

        if (q_out != nullptr) {
            if (q_out != &u && q_out->m_capacity < (t - 1U) * n) {
                q_out->reserve((t - 1U) * n);
            }
            q_out->m_size = static_cast<std::uint32_t>((t - 1U) * n);
            BigIntStorage::zero_limbs(q_out->as_span());
        }

        for (std::size_t i{t - 1U}; i > 0U; --i) {
            const std::size_t block_idx{i - 1U};
            const std::size_t start{block_idx * n};
            std::size_t a_block_len{0};
            if (start < u_shifted_len) {
                a_block_len = std::min(n, u_shifted_len - start);
            }

            BigIntStorage::zero_limbs(z);
            if (a_block_len > 0U) {
                BigIntStorage::copy_limbs(z, u_shifted.subspan(start, a_block_len));
            }
            if (r_cur_len > 0U) {
                BigIntStorage::copy_limbs(z.subspan(n), r_cur.subspan(0U, r_cur_len));
            }
            std::size_t z_len{n + r_cur_len};
            while (z_len > 0U && z[z_len - 1U] == 0ULL) --z_len;

            std::size_t q_step_len{0};
            std::size_t r_next_len{0};
            bz_div2by1_raw(q_step, &q_step_len, r_next, &r_next_len, z.subspan(0U, z_len), v_shifted.subspan(0U, n), n, arena);

            BigIntStorage::zero_limbs(r_cur);
            if (r_next_len > 0U) {
                BigIntStorage::copy_limbs(r_cur, r_next.subspan(0U, r_next_len));
            }
            r_cur_len = r_next_len;

            if (q_out != nullptr && q_step_len > 0U) {
                BigIntStorage::copy_limbs(q_out->as_span().subspan((i - 1U) * n), q_step.subspan(0U, q_step_len));
            }
        }

        if (q_out != nullptr) {
            q_out->m_size = static_cast<std::uint32_t>((t - 1U) * n);
            q_out->m_sign = (q_out->m_size > 0U) ? 1 : 0;
            q_out->normalize();
        }
        if (r_out != nullptr) {
            if (r_out != &u && r_out->m_capacity < n) {
                r_out->reserve(n);
            }
            if (sigma > 0U) {
                const std::size_t limb_s{sigma / 64U};
                const auto bit_s{static_cast<std::uint32_t>(sigma % 64U)};
                if (r_cur_len <= limb_s) {
                    r_out->m_size = 0;
                    r_out->m_sign = 0;
                } else {
                    std::size_t out_len{r_cur_len - limb_s};
                    const std::span<std::uint64_t> rd{r_out->capacity_span()};
                    const std::span<const std::uint64_t> src{r_cur.subspan(limb_s, out_len)};
                    if (bit_s == 0U) {
                        BigIntStorage::copy_limbs(rd, src);
                    } else {
                        for (std::size_t k{0}; k + 1U < out_len; ++k) {
                            rd[k] = shrd64(src[k + 1U], src[k], bit_s);
                        }
                        rd[out_len - 1U] = src[out_len - 1U] >> bit_s;
                    }
                    while (out_len > 0U && rd[out_len - 1U] == 0ULL) --out_len;
                    r_out->m_size = static_cast<std::uint32_t>(out_len);
                    r_out->m_sign = (out_len > 0U) ? 1 : 0;
                }
            } else {
                if (r_cur_len > 0U) {
                    BigIntStorage::copy_limbs(r_out->capacity_span(), r_cur.subspan(0U, r_cur_len));
                    r_out->m_size = static_cast<std::uint32_t>(r_cur_len);
                    r_out->m_sign = 1;
                } else {
                    r_out->m_size = 0;
                    r_out->m_sign = 0;
                }
            }
            r_out->normalize();
        }
    }

    static NUMERIC_CONSTEXPR_20 auto div_qr(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(&q, &r, u, v);
    }

    static NUMERIC_CONSTEXPR_20 auto div_q(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(&q, nullptr, u, v);
    }

    static NUMERIC_CONSTEXPR_20 auto div_r(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(nullptr, &r, u, v);
    }

    static NUMERIC_CONSTEXPR_20 auto div_qr_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(&q, &r, u, v);
        if (q.m_size > 0U) {
            q.m_sign = static_cast<std::int8_t>(u.m_sign * v.m_sign);
        }
        if (r.m_size > 0U) {
            r.m_sign = u.m_sign;
        }
    }

    static NUMERIC_CONSTEXPR_20 auto div_q_signed(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(&q, nullptr, u, v);
        if (q.m_size > 0U) {
            q.m_sign = static_cast<std::int8_t>(u.m_sign * v.m_sign);
        }
    }

    static NUMERIC_CONSTEXPR_20 auto div_r_signed(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(nullptr, &r, u, v);
        if (r.m_size > 0U) {
            r.m_sign = u.m_sign;
        }
    }

    static NUMERIC_CONSTEXPR_20 auto div_mod_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_qr_signed(q, r, u, v);
    }

    static NUMERIC_CONSTEXPR_20 auto div_mod_core(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) -> void {
        div_mod_core(&q, &r, u, v);
    }
};

// ----------------------------------------------------------------------------
// Burnikel–Ziegler Recursive Implementation / Burnikel-Ziegler 遞迴實作
// ----------------------------------------------------------------------------

inline auto BigIntDivision::bz_div3by2_raw(
    const std::span<std::uint64_t> q, std::size_t* const q_len,
    const std::span<std::uint64_t> r, std::size_t* const r_len,
    const std::span<const std::uint64_t> a,
    const std::span<const std::uint64_t> b, const std::size_t k,
    ScratchArena& arena) -> void
{
    if (a.empty()) {
        if (!q.empty()) BigIntStorage::zero_limbs(q.subspan(0U, k));
        if (q_len != nullptr) *q_len = 0U;
        if (!r.empty()) BigIntStorage::zero_limbs(r.subspan(0U, 2U * k));
        if (r_len != nullptr) *r_len = 0U;
        return;
    }

    const std::span<const std::uint64_t> b1{b.subspan(k, k)};
    std::size_t b0_len{k};
    while (b0_len > 0U && b[b0_len - 1U] == 0ULL) --b0_len;
    const std::span<const std::uint64_t> b0{b.subspan(0U, b0_len)};

    std::size_t a_hi_len{(a.size() > k) ? (a.size() - k) : 0U};
    while (a_hi_len > 0U && a[k + a_hi_len - 1U] == 0ULL) --a_hi_len;
    const std::span<const std::uint64_t> a_hi{(a.size() > k) ? a.subspan(k, a_hi_len) : std::span<const std::uint64_t>{}};

    std::size_t a_lo_len{std::min(a.size(), k)};
    while (a_lo_len > 0U && a[a_lo_len - 1U] == 0ULL) --a_lo_len;
    const std::span<const std::uint64_t> a_lo{a.subspan(0U, a_lo_len)};

    const ScratchArena::Scope scope{arena};

    const std::span<std::uint64_t> q_hat{arena.allocate(k + 1U)};
    const std::span<std::uint64_t> r_hat{arena.allocate(2U * k + 2U)};
    BigIntStorage::zero_limbs(q_hat);
    BigIntStorage::zero_limbs(r_hat);
    std::size_t q_hat_len{0};
    std::size_t r_hat_len{0};

    if (a_hi.empty()) {
        q_hat_len = 0U;
        r_hat_len = 0U;
    } else {
        bool a_hi_ge_b1_beta_k{false};
        if (a_hi_len > 2U * k) {
            a_hi_ge_b1_beta_k = true;
        } else if (a_hi_len == 2U * k) {
            a_hi_ge_b1_beta_k = (compare_unsigned(a_hi.subspan(k, k), b1) >= 0);
        }

        if (a_hi_ge_b1_beta_k) {
            std::ranges::fill(q_hat.subspan(0U, k), 0xFFFFFFFFFFFFFFFFULL);
            q_hat_len = k;

            const std::size_t lo_copy{std::min(a_hi_len, k)};
            if (lo_copy > 0U) {
                BigIntStorage::copy_limbs(r_hat, a_hi.subspan(0U, lo_copy));
            }
            const std::uint64_t carry{add_to_raw(r_hat.subspan(0U, k), b1)};
            r_hat[k] = carry;
            if (a_hi_len > k) {
                const std::size_t hi_part_len{a_hi_len - k};
                if (compare_unsigned(a_hi.subspan(k, hi_part_len), b1) > 0) {
                    const std::span<std::uint64_t> hi_diff{arena.allocate(hi_part_len + 1U)};
                    const std::size_t hd_len{sub_limbs_diff_raw(hi_diff, a_hi.subspan(k, hi_part_len), b1)};
                    (void)add_to_raw(r_hat.subspan(k, k + 2U), hi_diff.subspan(0U, hd_len));
                }
            }
            r_hat_len = 2U * k + 2U;
            while (r_hat_len > 0U && r_hat[r_hat_len - 1U] == 0ULL) --r_hat_len;
        } else {
            bz_div2by1_raw(q_hat, &q_hat_len, r_hat, &r_hat_len, a_hi, b1, k, arena);
        }
    }

    const std::span<std::uint64_t> d{arena.allocate(2U * k + 1U)};
    BigIntStorage::zero_limbs(d);
    std::size_t d_len{0};
    if (q_hat_len > 0U && !b0.empty()) {
        mul_slice_helper(d, q_hat.subspan(0U, q_hat_len), b0, arena);
        d_len = q_hat_len + b0.size();
        while (d_len > 0U && d[d_len - 1U] == 0ULL) --d_len;
    }

    const std::span<std::uint64_t> r_prime{arena.allocate(3U * k + 4U)};
    BigIntStorage::zero_limbs(r_prime);
    if (!a_lo.empty()) {
        BigIntStorage::copy_limbs(r_prime, a_lo);
    }
    if (r_hat_len > 0U) {
        BigIntStorage::copy_limbs(r_prime.subspan(k), r_hat.subspan(0U, r_hat_len));
    }
    std::size_t r_prime_len{k + r_hat_len};
    while (r_prime_len > 0U && r_prime[r_prime_len - 1U] == 0ULL) --r_prime_len;

    while (compare_unsigned(r_prime.subspan(0U, r_prime_len), d.subspan(0U, d_len)) < 0) {
        std::uint8_t borrow{1U};
        for (std::size_t i{0}; i < k && borrow != 0U; ++i) {
            borrow = sbb64(borrow, q_hat[i], 0ULL, q_hat[i]);
        }
        while (q_hat_len > 0U && q_hat[q_hat_len - 1U] == 0ULL) --q_hat_len;

        (void)add_to_raw(r_prime, b.subspan(0U, 2U * k));
        r_prime_len = 3U * k + 4U;
        while (r_prime_len > 0U && r_prime[r_prime_len - 1U] == 0ULL) --r_prime_len;
    }

    if (!r.empty()) {
        BigIntStorage::zero_limbs(r.subspan(0U, 2U * k));
        const std::size_t rl{sub_limbs_diff_raw(r, r_prime.subspan(0U, r_prime_len), d.subspan(0U, d_len))};
        if (r_len != nullptr) *r_len = rl;
    } else if (r_len != nullptr) {
        *r_len = 0U;
    }

    if (!q.empty()) {
        BigIntStorage::zero_limbs(q.subspan(0U, k));
        if (q_hat_len > 0U) {
            BigIntStorage::copy_limbs(q, q_hat.subspan(0U, q_hat_len));
        }
        if (q_len != nullptr) *q_len = q_hat_len;
    } else if (q_len != nullptr) {
        *q_len = 0U;
    }
}

inline auto BigIntDivision::bz_div2by1_raw(
    const std::span<std::uint64_t> q, std::size_t* const q_len,
    const std::span<std::uint64_t> r, std::size_t* const r_len,
    const std::span<const std::uint64_t> a,
    const std::span<const std::uint64_t> b, const std::size_t n,
    ScratchArena& arena) -> void
{
    if (a.empty()) {
        if (!q.empty()) BigIntStorage::zero_limbs(q.subspan(0U, n));
        if (q_len != nullptr) *q_len = 0U;
        if (!r.empty()) BigIntStorage::zero_limbs(r.subspan(0U, n));
        if (r_len != nullptr) *r_len = 0U;
        return;
    }

    if (n < BZ_THRESHOLD || (n % 2U != 0U)) {
        const std::size_t total_scratch{(n + 1U) + (a.size() + 2U)};
        if (!q.empty()) BigIntStorage::zero_limbs(q.subspan(0U, n));
        if (!r.empty()) BigIntStorage::zero_limbs(r.subspan(0U, n));
        if (total_scratch <= 512U) {
            std::array<std::uint64_t, 512> stack_scratch;
            div_knuth_raw(q, q_len, r, r_len, a, b.subspan(0U, n), stack_scratch);
        } else {
            const ScratchArena::Scope scope{arena};
            const std::span<std::uint64_t> scratch_span{arena.allocate(total_scratch)};
            div_knuth_raw(q, q_len, r, r_len, a, b.subspan(0U, n), scratch_span);
        }
        return;
    }

    const std::size_t k{n / 2U};
    std::size_t a_hi_len{(a.size() > k) ? (a.size() - k) : 0U};
    while (a_hi_len > 0U && a[k + a_hi_len - 1U] == 0ULL) --a_hi_len;
    const std::span<const std::uint64_t> a_hi{(a.size() > k) ? a.subspan(k, a_hi_len) : std::span<const std::uint64_t>{}};

    std::size_t a_lo_len{std::min(a.size(), k)};
    while (a_lo_len > 0U && a[a_lo_len - 1U] == 0ULL) --a_lo_len;
    const std::span<const std::uint64_t> a_lo{a.subspan(0U, a_lo_len)};

    const ScratchArena::Scope scope{arena};

    const std::span<std::uint64_t> q1{arena.allocate(k + 1U)};
    const std::span<std::uint64_t> r1{arena.allocate(2U * k + 1U)};
    std::size_t q1_len{0};
    std::size_t r1_len{0};

    bz_div3by2_raw(q1, &q1_len, r1, &r1_len, a_hi, b, k, arena);

    const std::span<std::uint64_t> a_step2{arena.allocate(3U * k + 1U)};
    BigIntStorage::zero_limbs(a_step2);
    if (!a_lo.empty()) {
        BigIntStorage::copy_limbs(a_step2, a_lo);
    }
    if (r1_len > 0U) {
        BigIntStorage::copy_limbs(a_step2.subspan(k), r1.subspan(0U, r1_len));
    }
    std::size_t a_step2_len{k + r1_len};
    while (a_step2_len > 0U && a_step2[a_step2_len - 1U] == 0ULL) --a_step2_len;

    const std::span<std::uint64_t> q0{arena.allocate(k + 1U)};
    const std::span<std::uint64_t> r0{arena.allocate(2U * k + 1U)};
    std::size_t q0_len{0};
    std::size_t r0_len{0};

    bz_div3by2_raw(q0, &q0_len, r0, &r0_len, a_step2.subspan(0U, a_step2_len), b, k, arena);

    if (!q.empty()) {
        BigIntStorage::zero_limbs(q.subspan(0U, 2U * k));
        if (q0_len > 0U) {
            BigIntStorage::copy_limbs(q, q0.subspan(0U, std::min(q0_len, k)));
        }
        if (q1_len > 0U) {
            BigIntStorage::copy_limbs(q.subspan(k), q1.subspan(0U, std::min(q1_len, k)));
        }
        std::size_t ql{2U * k};
        while (ql > 0U && q[ql - 1U] == 0ULL) --ql;
        if (q_len != nullptr) *q_len = ql;
    } else if (q_len != nullptr) {
        *q_len = 0U;
    }

    if (!r.empty()) {
        BigIntStorage::zero_limbs(r.subspan(0U, 2U * k));
        if (r0_len > 0U) {
            BigIntStorage::copy_limbs(r, r0.subspan(0U, std::min(r0_len, 2U * k)));
        }
        if (r_len != nullptr) *r_len = std::min(r0_len, 2U * k);
    } else if (r_len != nullptr) {
        *r_len = 0U;
    }
}

} // namespace detail
} // namespace numeric

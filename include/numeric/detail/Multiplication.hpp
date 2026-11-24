// filepath: include/numeric/detail/Multiplication.hpp
/**
 * @file Multiplication.hpp
 * @brief High-performance arbitrary-precision multiplication algorithms for CPP-BigInt.
 *        Implements Schoolbook, Block Karatsuba, subtraction-variant Karatsuba, Toom-Cook 3-way,
 *        and the pointer-stable chunked ScratchArena allocator over bounds-safe std::span slices.
 *        高效能任意精度乘法演算法模組，整合 std::span 邊界安全介面與指標穩定分塊 ScratchArena。
 *        涵蓋 Schoolbook、非對稱 Block Karatsuba、減法變體 Karatsuba 與 Toom-Cook 3 五點插值法。
 */
#pragma once

#include "Bitwise.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// Arbitrary-precision multiplication core inheriting BigIntBitwise.
/// BigInt 乘法運算核心類別，繼承自 BigIntBitwise。
/// </summary>
struct BigIntMultiplication : public BigIntBitwise {
    static constexpr std::size_t KARATSUBA_THRESHOLD{16U};
#ifndef NUMERIC_TOOM3_THRESHOLD
    static constexpr std::size_t TOOM3_THRESHOLD{64U}; // 4,096-bit zero-heap Toom-3 threshold / 4096 位元零堆積門檻
#else
    static constexpr std::size_t TOOM3_THRESHOLD{NUMERIC_TOOM3_THRESHOLD};
#endif

    /// <summary>
    /// Raw Schoolbook multiplication: out = a * b.
    /// First pass uses mul_1_raw directly to avoid redundant zero-fill and accumulation reads.
    /// 原生 Schoolbook 乘法：out = a * b。首趟採用 mul_1_raw 直寫輸出消除零填充與重複累加。
    /// </summary>
    /// <param name="out">Output span (size >= a.size() + b.size()). / 輸出乘積 span。</param>
    /// <param name="a">First factor span. / 乘數 a span。</param>
    /// <param name="b">Second factor span. / 乘數 b span。</param>
    static NUMERIC_CONSTEXPR_20 auto mul_schoolbook_raw(
        const std::span<std::uint64_t> out,
        std::span<const std::uint64_t> a,
        std::span<const std::uint64_t> b) noexcept -> void
    {
        if (a.empty() || b.empty()) {
            BigIntStorage::zero_limbs(out.subspan(0U, a.size() + b.size()));
            return;
        }
        assert(out.size() >= a.size() + b.size());
        if (a.size() == 1U && b.size() == 1U) {
            std::uint64_t hi{0};
            out[0] = mul64_wide(a[0], b[0], hi);
            out[1] = hi;
            return;
        }
        if (a.size() > b.size()) {
            const auto tmp{a};
            a = b;
            b = tmp;
        }

        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        out[b_len] = mul_1_raw(out, b, a[0]);
        if (a_len > 1U) {
            BigIntStorage::zero_limbs(out.subspan(b_len + 1U, a_len - 1U));
            for (std::size_t i{1U}; i < a_len; ++i) {
                if (a[i] == 0ULL) {
                    continue;
                }
                const std::uint64_t carry{addmul_1_raw(out.subspan(i), b, a[i])};
                out[i + b_len] += carry;
            }
        }
    }

    /// <summary>
    /// Raw in-place limb addition: dst += src. Returns final carry limb.
    /// 原生原地加法：dst += src，回傳最終溢出進位。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto add_to_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src) noexcept -> std::uint64_t
    {
        std::uint8_t carry{0};
        const std::size_t dst_len{dst.size()};
        const std::size_t min_len{std::min(dst_len, src.size())};
        for (std::size_t i{0}; i < min_len; ++i) {
            carry = adc64(carry, dst[i], src[i], dst[i]);
        }
        for (std::size_t i{min_len}; i < dst_len && carry != 0U; ++i) {
            carry = adc64(carry, dst[i], 0ULL, dst[i]);
        }
        return carry;
    }

    /// <summary>
    /// Raw in-place limb subtraction: dst -= src. Returns final borrow limb.
    /// 原生原地減法：dst -= src，回傳最終借位。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto sub_from_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src) noexcept -> std::uint64_t
    {
        std::uint8_t borrow{0};
        const std::size_t dst_len{dst.size()};
        const std::size_t min_len{std::min(dst_len, src.size())};
        for (std::size_t i{0}; i < min_len; ++i) {
            borrow = sbb64(borrow, dst[i], src[i], dst[i]);
        }
        for (std::size_t i{min_len}; i < dst_len && borrow != 0U; ++i) {
            borrow = sbb64(borrow, dst[i], 0ULL, dst[i]);
        }
        return borrow;
    }

    /// <summary>
    /// Raw magnitude difference: out = a - b (Precondition: a >= b). Returns normalized length.
    /// 原生相減差值絕對值：out = a - b（前置條件 a >= b），回傳正規化長度。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto sub_limbs_diff_raw(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b) noexcept -> std::size_t
    {
        const std::size_t a_len{a.size()};
        const std::size_t min_len{std::min(a_len, b.size())};
        assert(out.size() >= a_len);

        std::uint8_t borrow{0};
        for (std::size_t i{0}; i < min_len; ++i) {
            borrow = sbb64(borrow, a[i], b[i], out[i]);
        }
        for (std::size_t i{min_len}; i < a_len; ++i) {
            borrow = sbb64(borrow, a[i], 0ULL, out[i]);
        }
        std::size_t len{a_len};
        while (len > 0U && out[len - 1U] == 0ULL) {
            --len;
        }
        return len;
    }

    /// <summary>
    /// Raw limb addition: dst = a + b. Returns normalized length.
    /// 原生加法：dst = a + b，回傳正規化長度。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto add_limbs_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b) noexcept -> std::size_t
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        const std::size_t max_len{std::max(a_len, b_len)};
        const std::size_t min_len{std::min(a_len, b_len)};
        const std::span<const std::uint64_t> longer{(a_len >= b_len) ? a : b};
        assert(dst.size() >= max_len);

        std::uint8_t carry{0};
        for (std::size_t i{0}; i < min_len; ++i) {
            carry = adc64(carry, a[i], b[i], dst[i]);
        }
        for (std::size_t i{min_len}; i < max_len; ++i) {
            carry = adc64(carry, longer[i], 0ULL, dst[i]);
        }
        if (carry != 0U) {
            assert(dst.size() > max_len);
            dst[max_len] = carry;
            return max_len + 1U;
        }
        std::size_t r_len{max_len};
        while (r_len > 0U && dst[r_len - 1U] == 0ULL) {
            --r_len;
        }
        return r_len;
    }

    /// <summary>
    /// Raw limb subtraction: dst = a - b (Precondition: a >= b). Returns normalized length.
    /// 原生減法：dst = a - b（前置條件 a >= b），回傳正規化長度。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto sub_limbs_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b) noexcept -> std::size_t
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        assert(dst.size() >= a_len && a_len >= b_len);

        std::uint8_t borrow{0};
        for (std::size_t i{0}; i < b_len; ++i) {
            borrow = sbb64(borrow, a[i], b[i], dst[i]);
        }
        for (std::size_t i{b_len}; i < a_len; ++i) {
            borrow = sbb64(borrow, a[i], 0ULL, dst[i]);
        }
        std::size_t r_len{a_len};
        while (r_len > 0U && dst[r_len - 1U] == 0ULL) {
            --r_len;
        }
        return r_len;
    }

    /// <summary>
    /// In-place 1-bit right shift: buf >>= 1.
    /// 原地右移 1 位：buf >>= 1。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto shift_right_1_raw(const std::span<std::uint64_t> buf) noexcept -> void {
        std::uint64_t carry{0};
        for (std::size_t i{buf.size()}; i > 0U; --i) {
            const std::uint64_t cur{buf[i - 1U]};
            buf[i - 1U] = (cur >> 1U) | carry;
            carry = cur << 63U;
        }
    }

    /// <summary>
    /// Left bit shift within limb word: dst = src << shift (1 <= shift < 64). Returns new length.
    /// 原生左移 bits 位元：dst = src << shift（1 <= shift < 64），回傳新長度。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto shift_left_bits_raw(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src,
        const std::uint32_t shift) noexcept -> std::size_t
    {
        const std::size_t len{src.size()};
        if (len == 0U) {
            return 0U;
        }
        assert(dst.size() >= len && shift > 0U && shift < 64U);
        std::uint64_t carry{0};
        for (std::size_t i{0}; i < len; ++i) {
            const std::uint64_t cur{src[i]};
            dst[i] = (cur << shift) | carry;
            carry = cur >> (64U - shift);
        }
        if (carry != 0ULL) {
            assert(dst.size() > len);
            dst[len] = carry;
            return len + 1U;
        }
        return len;
    }

    /// <summary>
    /// Thread-local chunked bump allocator providing pointer-stable scratch spans for deep recursions.
    /// 線程局部暫存記憶體池（具備指標穩定分塊與 RAII Scope 自動回滾功能）。
    /// </summary>
    class ScratchArena final {
    public:
        NUMERIC_NODISCARD static auto instance() noexcept -> ScratchArena& {
            static thread_local ScratchArena arena{};
            return arena;
        }

        struct Marker final {
            std::size_t chunk_idx{0};
            std::size_t offset{0};
        };

        class Scope final {
        public:
            explicit NUMERIC_CONSTEXPR_20 Scope(ScratchArena& arena, const bool active = true) noexcept
                : m_arena{active ? &arena : nullptr},
                  m_saved{active ? arena.get_marker() : Marker{0U, 0U}} {}

            NUMERIC_CONSTEXPR_20 ~Scope() noexcept {
                if (m_arena != nullptr) {
                    m_arena->restore_marker(m_saved);
                }
            }

            Scope(const Scope&) = delete;
            auto operator=(const Scope&) -> Scope& = delete;
            Scope(Scope&&) = delete;
            auto operator=(Scope&&) -> Scope& = delete;

        private:
            ScratchArena* m_arena{nullptr};
            Marker m_saved{};
        };

        NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto get_marker() const noexcept -> Marker {
            return {m_current_chunk, m_current_offset};
        }

        NUMERIC_CONSTEXPR_20 auto restore_marker(const Marker m) noexcept -> void {
            m_current_chunk = m.chunk_idx;
            m_current_offset = m.offset;
        }

        /// <summary>
        /// Allocates a contiguous scratch span of count 64-bit limbs.
        /// 配置包含 count 個 64 位元 limbs 的連續暫存 span。
        /// </summary>
        NUMERIC_NODISCARD auto allocate(std::size_t count) -> std::span<std::uint64_t> {
            if (count == 0U) {
                count = 1U;
            }

            if (m_current_chunk < m_chunks.size()) {
                auto& cur_chunk{m_chunks[m_current_chunk]};
                if (m_current_offset + count <= cur_chunk.size) {
                    const std::size_t offset{m_current_offset};
                    m_current_offset += count;
                    return std::span<std::uint64_t>{cur_chunk.buffer.get(), cur_chunk.size}.subspan(offset, count);
                }
                ++m_current_chunk;
                m_current_offset = 0U;
            }

            if (m_current_chunk < m_chunks.size()) {
                auto& cur_chunk{m_chunks[m_current_chunk]};
                if (cur_chunk.size >= count) {
                    m_current_offset = count;
                    return std::span<std::uint64_t>{cur_chunk.buffer.get(), cur_chunk.size}.subspan(0U, count);
                }
                const std::size_t new_cap{(std::max)(count, DEFAULT_CHUNK_SIZE)};
                cur_chunk.buffer = std::make_unique<std::uint64_t[]>(new_cap);
                cur_chunk.size = new_cap;
                m_current_offset = count;
                return std::span<std::uint64_t>{cur_chunk.buffer.get(), cur_chunk.size}.subspan(0U, count);
            }

            const std::size_t new_cap{(std::max)(count, DEFAULT_CHUNK_SIZE)};
            Chunk chunk{};
            chunk.buffer = std::make_unique<std::uint64_t[]>(new_cap);
            chunk.size = new_cap;
            m_chunks.push_back(std::move(chunk));
            auto& back_chunk{m_chunks.back()};
            m_current_offset = count;
            return std::span<std::uint64_t>{back_chunk.buffer.get(), back_chunk.size}.subspan(0U, count);
        }

    private:
        ScratchArena() = default;
        static constexpr std::size_t DEFAULT_CHUNK_SIZE{65536U};

        struct Chunk final {
            std::unique_ptr<std::uint64_t[]> buffer{};
            std::size_t size{0};
        };

        std::vector<Chunk> m_chunks{};
        std::size_t m_current_chunk{0};
        std::size_t m_current_offset{0};
    };

    /// <summary>
    /// Subtraction-variant scratchpad Karatsuba multiplication: out = a * b.
    /// 減法變體 Scratchpad Karatsuba 核心演算法：out = a * b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_karatsuba_raw(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b,
        const std::span<std::uint64_t> scratch) noexcept -> void
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        if (a_len < KARATSUBA_THRESHOLD || b_len < KARATSUBA_THRESHOLD) {
            mul_schoolbook_raw(out, a, b);
            return;
        }

        const std::size_t n{(a_len > b_len) ? a_len : b_len};
        const std::size_t m{(n + 1U) / 2U};

        std::size_t a0_len{std::min(a_len, m)};
        while (a0_len > 0U && a[a0_len - 1U] == 0ULL) --a0_len;
        const std::span<const std::uint64_t> a0{a.subspan(0U, a0_len)};

        std::size_t a1_len{(a_len > m) ? (a_len - m) : 0U};
        while (a1_len > 0U && a[m + a1_len - 1U] == 0ULL) --a1_len;
        const std::span<const std::uint64_t> a1{(a_len > m) ? a.subspan(m, a1_len) : std::span<const std::uint64_t>{}};

        std::size_t b0_len{std::min(b_len, m)};
        while (b0_len > 0U && b[b0_len - 1U] == 0ULL) --b0_len;
        const std::span<const std::uint64_t> b0{b.subspan(0U, b0_len)};

        std::size_t b1_len{(b_len > m) ? (b_len - m) : 0U};
        while (b1_len > 0U && b[m + b1_len - 1U] == 0ULL) --b1_len;
        const std::span<const std::uint64_t> b1{(b_len > m) ? b.subspan(m, b1_len) : std::span<const std::uint64_t>{}};

        const std::span<std::uint64_t> da{scratch.subspan(0U, m)};
        const std::span<std::uint64_t> db{scratch.subspan(m, m)};
        const std::span<std::uint64_t> p{scratch.subspan(2U * m, 2U * m)};
        const std::span<std::uint64_t> t{scratch.subspan(4U * m, 2U * m + 2U)};
        const std::span<std::uint64_t> next_scratch{scratch.subspan(6U * m + 2U)};

        BigIntStorage::zero_limbs(out.subspan(0U, a_len + b_len));
        std::size_t z0_len{0};
        if (!a0.empty() && !b0.empty()) {
            z0_len = a0.size() + b0.size();
            mul_karatsuba_raw(out, a0, b0, next_scratch);
        }

        std::size_t z2_len{0};
        if (!a1.empty() && !b1.empty()) {
            z2_len = a1.size() + b1.size();
            mul_karatsuba_raw(out.subspan(2U * m), a1, b1, next_scratch);
        }

        const std::int32_t cmp_a{compare_unsigned(a0, a1)};
        std::int8_t s_a{0};
        std::size_t da_len{0};
        if (cmp_a > 0) {
            s_a = 1;
            da_len = sub_limbs_diff_raw(da, a0, a1);
        } else if (cmp_a < 0) {
            s_a = -1;
            da_len = sub_limbs_diff_raw(da, a1, a0);
        }

        const std::int32_t cmp_b{compare_unsigned(b0, b1)};
        std::int8_t s_b{0};
        std::size_t db_len{0};
        if (cmp_b > 0) {
            s_b = 1;
            db_len = sub_limbs_diff_raw(db, b0, b1);
        } else if (cmp_b < 0) {
            s_b = -1;
            db_len = sub_limbs_diff_raw(db, b1, b0);
        }

        std::int8_t s_p{static_cast<std::int8_t>(s_a * s_b)};
        std::size_t p_len{0};
        if (s_p != 0 && da_len > 0U && db_len > 0U) {
            p_len = da_len + db_len;
            mul_karatsuba_raw(p, da.subspan(0U, da_len), db.subspan(0U, db_len), next_scratch);
        } else {
            s_p = 0;
        }

        if (z0_len > 2U * m) {
            z0_len = 2U * m;
        }
        if (z0_len > 0U) {
            BigIntStorage::copy_limbs(t, out.subspan(0U, z0_len));
        }
        BigIntStorage::zero_limbs(t.subspan(z0_len));

        if (z2_len > 0U) {
            (void)add_to_raw(t, out.subspan(2U * m, z2_len));
        }

        if (s_p > 0 && p_len > 0U) {
            (void)sub_from_raw(t, p.subspan(0U, p_len));
        } else if (s_p < 0 && p_len > 0U) {
            (void)add_to_raw(t, p.subspan(0U, p_len));
        }

        (void)add_to_raw(out.subspan(m, (a_len + b_len) - m), t);
    }

    /// <summary>
    /// Exact division by 3: out = out / 3 (Precondition: out is divisible by 3).
    /// 精確除以 3：out = out / 3（前置合約：out 可被 3 整除）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto div_exact_3_raw(const std::span<std::uint64_t> out) noexcept -> void {
        std::uint64_t rem{0};
        for (std::size_t i{out.size()}; i > 0U; --i) {
            std::uint64_t next_rem{0};
            out[i - 1U] = div128_64(rem, out[i - 1U], 3ULL, next_rem);
            rem = next_rem;
        }
    }

    /// <summary>
    /// Unbalanced Block Karatsuba multiplication reducing asymmetric O(N*M) to O(N*M^0.585).
    /// 非平衡乘法分塊演算法 (Block Karatsuba)：將長乘數依短乘數長度切塊乘加。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_block_karatsuba_raw(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b,
        const std::span<std::uint64_t> scratch) noexcept -> void
    {
        if (b.size() > a.size()) {
            mul_block_karatsuba_raw(out, b, a, scratch);
            return;
        }
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        BigIntStorage::zero_limbs(out.subspan(0U, a_len + b_len));
        if (b_len < KARATSUBA_THRESHOLD || b_len < 4U) {
            mul_schoolbook_raw(out, a, b);
            return;
        }

        const std::size_t block_sz{b_len};
        const std::size_t num_blocks{a_len / block_sz};
        const std::size_t rem{a_len % block_sz};

        const std::span<std::uint64_t> prod_buf{scratch.subspan(0U, 2U * block_sz + 8U)};
        const std::span<std::uint64_t> next_scratch{scratch.subspan(2U * block_sz + 8U)};

        for (std::size_t i{0}; i < num_blocks; ++i) {
            const std::span<const std::uint64_t> a_block{a.subspan(i * block_sz, block_sz)};
            mul_dispatch_raw(prod_buf, a_block, b, next_scratch);
            const std::size_t out_offset{i * block_sz};
            (void)add_to_raw(out.subspan(out_offset, (a_len + b_len) - out_offset), prod_buf.subspan(0U, 2U * block_sz));
        }

        if (rem > 0U) {
            const std::span<const std::uint64_t> a_block{a.subspan(num_blocks * block_sz, rem)};
            if (rem < 4U) {
                mul_schoolbook_raw(prod_buf, a_block, b);
            } else {
                mul_dispatch_raw(prod_buf, a_block, b, next_scratch);
            }
            const std::size_t out_offset{num_blocks * block_sz};
            (void)add_to_raw(out.subspan(out_offset, (a_len + b_len) - out_offset), prod_buf.subspan(0U, rem + b_len));
        }
    }

    /// <summary>
    /// Recursive multiplication dispatcher selecting Schoolbook, Block Karatsuba, Karatsuba, or Toom-3.
    /// 乘法內部遞迴派發函式，依運算元規模分派至最佳演算法。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_dispatch_raw(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b,
        const std::span<std::uint64_t> scratch) noexcept -> void
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        const std::size_t n{std::max(a_len, b_len)};
        if (a_len == 0U || b_len == 0U) {
            BigIntStorage::zero_limbs(out.subspan(0U, a_len + b_len));
            return;
        }
        if (n < KARATSUBA_THRESHOLD || a_len < 4U || b_len < 4U) {
            mul_schoolbook_raw(out, a, b);
        } else if (a_len >= 2U * b_len || b_len >= 2U * a_len) {
            mul_block_karatsuba_raw(out, a, b, scratch);
        } else if (n < TOOM3_THRESHOLD) {
            mul_karatsuba_raw(out, a, b, scratch);
        } else {
            mul_toom3_raw(out, a, b, scratch);
        }
    }

    /// <summary>
    /// Production Toom-Cook 3-way (Toom-3) multiplication: out = a * b.
    /// Asymptotic complexity O(N^1.465) via 5-point evaluation and zero-heap raw limb interpolation.
    /// 生產級 Toom-Cook 3 乘法核心演算法，採用五點插值法與零堆積原生插值重構。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_toom3_raw(
        const std::span<std::uint64_t> out,
        const std::span<const std::uint64_t> a,
        const std::span<const std::uint64_t> b,
        const std::span<std::uint64_t> scratch) noexcept -> void
    {
        const std::size_t a_len{a.size()};
        const std::size_t b_len{b.size()};
        const std::size_t n{std::max(a_len, b_len)};
        const std::size_t m{(n + 2U) / 3U};

        std::size_t a0_len{std::min(a_len, m)};
        while (a0_len > 0U && a[a0_len - 1U] == 0ULL) --a0_len;
        const std::span<const std::uint64_t> a0{a.subspan(0U, a0_len)};

        std::size_t a1_len{(a_len > m) ? ((a_len < 2U * m) ? (a_len - m) : m) : 0U};
        while (a1_len > 0U && a[m + a1_len - 1U] == 0ULL) --a1_len;
        const std::span<const std::uint64_t> a1{(a_len > m) ? a.subspan(m, a1_len) : std::span<const std::uint64_t>{}};

        std::size_t a2_len{(a_len > 2U * m) ? (a_len - 2U * m) : 0U};
        while (a2_len > 0U && a[2U * m + a2_len - 1U] == 0ULL) --a2_len;
        const std::span<const std::uint64_t> a2{(a_len > 2U * m) ? a.subspan(2U * m, a2_len) : std::span<const std::uint64_t>{}};

        std::size_t b0_len{std::min(b_len, m)};
        while (b0_len > 0U && b[b0_len - 1U] == 0ULL) --b0_len;
        const std::span<const std::uint64_t> b0{b.subspan(0U, b0_len)};

        std::size_t b1_len{(b_len > m) ? ((b_len < 2U * m) ? (b_len - m) : m) : 0U};
        while (b1_len > 0U && b[m + b1_len - 1U] == 0ULL) --b1_len;
        const std::span<const std::uint64_t> b1{(b_len > m) ? b.subspan(m, b1_len) : std::span<const std::uint64_t>{}};

        std::size_t b2_len{(b_len > 2U * m) ? (b_len - 2U * m) : 0U};
        while (b2_len > 0U && b[2U * m + b2_len - 1U] == 0ULL) --b2_len;
        const std::span<const std::uint64_t> b2{(b_len > 2U * m) ? b.subspan(2U * m, b2_len) : std::span<const std::uint64_t>{}};

        const std::size_t ev_sz{m + 2U};
        const std::size_t pr_sz{2U * m + 4U};

        const std::span<std::uint64_t> p1{scratch.subspan(0U, ev_sz)};
        const std::span<std::uint64_t> q1{scratch.subspan(ev_sz, ev_sz)};
        const std::span<std::uint64_t> p_m1{scratch.subspan(2U * ev_sz, ev_sz)};
        const std::span<std::uint64_t> q_m1{scratch.subspan(3U * ev_sz, ev_sz)};
        const std::span<std::uint64_t> p2{scratch.subspan(4U * ev_sz, ev_sz)};
        const std::span<std::uint64_t> q2{scratch.subspan(5U * ev_sz, ev_sz)};

        const std::span<std::uint64_t> v1{scratch.subspan(6U * ev_sz, pr_sz)};
        const std::span<std::uint64_t> v_m1{scratch.subspan(6U * ev_sz + pr_sz, pr_sz)};
        const std::span<std::uint64_t> v2{scratch.subspan(6U * ev_sz + 2U * pr_sz, pr_sz)};
        const std::span<std::uint64_t> next_scratch{scratch.subspan(6U * ev_sz + 3U * pr_sz)};

        BigIntStorage::zero_limbs(scratch.subspan(0U, 6U * ev_sz + 3U * pr_sz));
        BigIntStorage::zero_limbs(out.subspan(0U, a_len + b_len));

        if (!a0.empty() && !b0.empty()) {
            mul_dispatch_raw(out, a0, b0, next_scratch);
        }

        const std::span<std::uint64_t> v_inf{out.subspan(4U * m)};
        if (!a2.empty() && !b2.empty()) {
            mul_dispatch_raw(v_inf, a2, b2, next_scratch);
        }

        if (!a0.empty()) {
            BigIntStorage::copy_limbs(p1, a0);
        }
        if (!a2.empty()) (void)add_to_raw(p1, a2);
        BigIntStorage::copy_limbs(p_m1, p1);
        if (!a1.empty()) (void)add_to_raw(p1, a1);

        std::int8_t s_a{1};
        std::size_t a0_a2_len{ev_sz};
        while (a0_a2_len > 0U && p_m1[a0_a2_len - 1U] == 0ULL) --a0_a2_len;
        const std::int32_t cmp_a{compare_unsigned(p_m1.subspan(0U, a0_a2_len), a1)};
        if (cmp_a >= 0) {
            if (!a1.empty()) (void)sub_from_raw(p_m1, a1);
            s_a = 1;
        } else {
            std::array<std::uint64_t, 256> tmp{};
            const std::span<std::uint64_t> t_span{(ev_sz <= 256U) ? std::span<std::uint64_t>{tmp}.subspan(0U, ev_sz) : next_scratch.subspan(0U, ev_sz)};
            BigIntStorage::copy_limbs(t_span, a1);
            BigIntStorage::zero_limbs(t_span.subspan(a1.size()));
            (void)sub_from_raw(t_span, p_m1.subspan(0U, a0_a2_len));
            BigIntStorage::copy_limbs(p_m1, t_span);
            s_a = -1;
        }

        if (!a2.empty()) {
            (void)add_to_raw(p2, a2);
            (void)add_to_raw(p2, a2);
        }
        if (!a1.empty()) (void)add_to_raw(p2, a1);
        std::uint64_t carry{0};
        for (std::size_t i{0}; i < ev_sz; ++i) {
            const std::uint64_t cur{p2[i]};
            p2[i] = (cur << 1U) | carry;
            carry = cur >> 63U;
        }
        if (!a0.empty()) (void)add_to_raw(p2, a0);

        if (!b0.empty()) {
            BigIntStorage::copy_limbs(q1, b0);
        }
        if (!b2.empty()) (void)add_to_raw(q1, b2);
        BigIntStorage::copy_limbs(q_m1, q1);
        if (!b1.empty()) (void)add_to_raw(q1, b1);

        std::int8_t s_b{1};
        std::size_t b0_b2_len{ev_sz};
        while (b0_b2_len > 0U && q_m1[b0_b2_len - 1U] == 0ULL) --b0_b2_len;
        const std::int32_t cmp_b{compare_unsigned(q_m1.subspan(0U, b0_b2_len), b1)};
        if (cmp_b >= 0) {
            if (!b1.empty()) (void)sub_from_raw(q_m1, b1);
            s_b = 1;
        } else {
            std::array<std::uint64_t, 256> tmp{};
            const std::span<std::uint64_t> t_span{(ev_sz <= 256U) ? std::span<std::uint64_t>{tmp}.subspan(0U, ev_sz) : next_scratch.subspan(0U, ev_sz)};
            BigIntStorage::copy_limbs(t_span, b1);
            BigIntStorage::zero_limbs(t_span.subspan(b1.size()));
            (void)sub_from_raw(t_span, q_m1.subspan(0U, b0_b2_len));
            BigIntStorage::copy_limbs(q_m1, t_span);
            s_b = -1;
        }

        if (!b2.empty()) {
            (void)add_to_raw(q2, b2);
            (void)add_to_raw(q2, b2);
        }
        if (!b1.empty()) (void)add_to_raw(q2, b1);
        carry = 0ULL;
        for (std::size_t i{0}; i < ev_sz; ++i) {
            const std::uint64_t cur{q2[i]};
            q2[i] = (cur << 1U) | carry;
            carry = cur >> 63U;
        }
        if (!b0.empty()) (void)add_to_raw(q2, b0);

        std::size_t p1_len{ev_sz}; while (p1_len > 0U && p1[p1_len - 1U] == 0ULL) --p1_len;
        std::size_t q1_len{ev_sz}; while (q1_len > 0U && q1[q1_len - 1U] == 0ULL) --q1_len;
        if (p1_len > 0U && q1_len > 0U) {
            mul_dispatch_raw(v1, p1.subspan(0U, p1_len), q1.subspan(0U, q1_len), next_scratch);
        }

        std::size_t pm1_len{ev_sz}; while (pm1_len > 0U && p_m1[pm1_len - 1U] == 0ULL) --pm1_len;
        std::size_t qm1_len{ev_sz}; while (qm1_len > 0U && q_m1[qm1_len - 1U] == 0ULL) --qm1_len;
        if (pm1_len > 0U && qm1_len > 0U) {
            mul_dispatch_raw(v_m1, p_m1.subspan(0U, pm1_len), q_m1.subspan(0U, qm1_len), next_scratch);
        }
        const std::int8_t s_vm1{static_cast<std::int8_t>(s_a * s_b)};

        std::size_t p2_len{ev_sz}; while (p2_len > 0U && p2[p2_len - 1U] == 0ULL) --p2_len;
        std::size_t q2_len{ev_sz}; while (q2_len > 0U && q2[q2_len - 1U] == 0ULL) --q2_len;
        if (p2_len > 0U && q2_len > 0U) {
            mul_dispatch_raw(v2, p2.subspan(0U, p2_len), q2.subspan(0U, q2_len), next_scratch);
        }

        const std::size_t sz{pr_sz + 4U};
        const std::span<std::uint64_t> t1{next_scratch.subspan(0U, sz)};
        const std::span<std::uint64_t> t2{next_scratch.subspan(sz, sz)};
        const std::span<std::uint64_t> c2{next_scratch.subspan(2U * sz, sz)};
        const std::span<std::uint64_t> v2_step{next_scratch.subspan(3U * sz, sz)};
        const std::span<std::uint64_t> v2_step2{next_scratch.subspan(4U * sz, sz)};
        const std::span<std::uint64_t> eight_v_inf{next_scratch.subspan(5U * sz, sz)};
        const std::span<std::uint64_t> two_c2{next_scratch.subspan(6U * sz, sz)};
        const std::span<std::uint64_t> c3{next_scratch.subspan(7U * sz, sz)};
        const std::span<std::uint64_t> c1{next_scratch.subspan(8U * sz, sz)};
        BigIntStorage::zero_limbs(next_scratch.subspan(0U, 9U * sz));

        std::size_t v0_len{2U * m};
        while (v0_len > 0U && out[v0_len - 1U] == 0ULL) --v0_len;
        const std::span<const std::uint64_t> v0_span{out.subspan(0U, v0_len)};

        std::size_t vinf_len{(a_len + b_len > 4U * m) ? (a_len + b_len - 4U * m) : 0U};
        while (vinf_len > 0U && v_inf[vinf_len - 1U] == 0ULL) --vinf_len;
        const std::span<const std::uint64_t> vinf_span{v_inf.subspan(0U, vinf_len)};

        std::size_t v1_len{pr_sz};
        while (v1_len > 0U && v1[v1_len - 1U] == 0ULL) --v1_len;
        const std::span<const std::uint64_t> v1_span{v1.subspan(0U, v1_len)};

        std::size_t vm1_len{pr_sz};
        while (vm1_len > 0U && v_m1[vm1_len - 1U] == 0ULL) --vm1_len;
        const std::span<const std::uint64_t> vm1_span{v_m1.subspan(0U, vm1_len)};

        std::size_t v2_len{pr_sz};
        while (v2_len > 0U && v2[v2_len - 1U] == 0ULL) --v2_len;
        const std::span<const std::uint64_t> v2_span{v2.subspan(0U, v2_len)};

        std::size_t t1_len{0};
        std::size_t t2_len{0};
        if (s_vm1 >= 0) {
            t1_len = add_limbs_raw(t1, v1_span, vm1_span);
            shift_right_1_raw(t1.subspan(0U, t1_len));
            while (t1_len > 0U && t1[t1_len - 1U] == 0ULL) --t1_len;

            t2_len = sub_limbs_raw(t2, v1_span, vm1_span);
            shift_right_1_raw(t2.subspan(0U, t2_len));
            while (t2_len > 0U && t2[t2_len - 1U] == 0ULL) --t2_len;
        } else {
            t1_len = sub_limbs_raw(t1, v1_span, vm1_span);
            shift_right_1_raw(t1.subspan(0U, t1_len));
            while (t1_len > 0U && t1[t1_len - 1U] == 0ULL) --t1_len;

            t2_len = add_limbs_raw(t2, v1_span, vm1_span);
            shift_right_1_raw(t2.subspan(0U, t2_len));
            while (t2_len > 0U && t2[t2_len - 1U] == 0ULL) --t2_len;
        }

        std::size_t c2_len{sub_limbs_raw(c2, t1.subspan(0U, t1_len), v0_span)};
        (void)sub_from_raw(c2.subspan(0U, c2_len), vinf_span);
        while (c2_len > 0U && c2[c2_len - 1U] == 0ULL) --c2_len;

        std::size_t v2_step_len{sub_limbs_raw(v2_step, v2_span, v0_span)};
        shift_right_1_raw(v2_step.subspan(0U, v2_step_len));
        while (v2_step_len > 0U && v2_step[v2_step_len - 1U] == 0ULL) --v2_step_len;

        const std::size_t v2_step2_len{sub_limbs_raw(v2_step2, v2_step.subspan(0U, v2_step_len), t2.subspan(0U, t2_len))};
        const std::size_t eight_vinf_len{shift_left_bits_raw(eight_v_inf, vinf_span, 3U)};
        (void)sub_from_raw(v2_step2.subspan(0U, v2_step2_len), eight_v_inf.subspan(0U, eight_vinf_len));
        std::size_t v2_step3_len{v2_step2_len};
        while (v2_step3_len > 0U && v2_step2[v2_step3_len - 1U] == 0ULL) --v2_step3_len;

        const std::size_t two_c2_len{shift_left_bits_raw(two_c2, c2.subspan(0U, c2_len), 1U)};
        const std::size_t three_c3_len{sub_limbs_raw(c3, v2_step2.subspan(0U, v2_step3_len), two_c2.subspan(0U, two_c2_len))};

        div_exact_3_raw(c3.subspan(0U, three_c3_len));
        std::size_t c3_len{three_c3_len};
        while (c3_len > 0U && c3[c3_len - 1U] == 0ULL) --c3_len;

        const std::size_t c1_len{sub_limbs_raw(c1, t2.subspan(0U, t2_len), c3.subspan(0U, c3_len))};

        if (c1_len > 0U) (void)add_to_raw(out.subspan(m, (a_len + b_len) - m), c1.subspan(0U, c1_len));
        if (c2_len > 0U) (void)add_to_raw(out.subspan(2U * m, (a_len + b_len) - 2U * m), c2.subspan(0U, c2_len));
        if (c3_len > 0U) (void)add_to_raw(out.subspan(3U * m, (a_len + b_len) - 3U * m), c3.subspan(0U, c3_len));
    }

    /// <summary>
    /// High-level Schoolbook multiplication dispatching into storage.
    /// 傳統 Schoolbook 長整數乘法演算法（包裝為儲存物件版本）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_schoolbook(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_size == 0U || b.m_size == 0U) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            tmp.resize(a.m_size + b.m_size, 0ULL);
            mul_schoolbook_raw(tmp.as_span(), a.as_span(), b.as_span());
            tmp.m_sign = 1;
            tmp.normalize();
            res = std::move(tmp);
        } else {
            res.resize(a.m_size + b.m_size, 0ULL);
            mul_schoolbook_raw(res.as_span(), a.as_span(), b.as_span());
            res.m_sign = 1;
            res.normalize();
        }
    }

    /// <summary>
    /// Single-limb multiplication with self-aliasing protection: res = a * b_val.
    /// 單肢段非平衡乘法：res = a * b_val（含自我別名防護）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_single_limb(
        BigIntStorage& res,
        const BigIntStorage& a,
        const std::uint64_t b_val,
        const std::int8_t sign) -> void
    {
        if (b_val == 0ULL || a.m_size == 0U) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (b_val == 1ULL) {
            if (&res != &a) {
                res = a;
            }
            res.m_sign = sign;
            return;
        }

        const std::size_t n{a.m_size};
        if (res.m_capacity < n + 1U) {
            res.reserve(n + 1U);
        }

        const std::span<const std::uint64_t> a_span{a.as_span()};
        const std::span<std::uint64_t> out_span{res.capacity_span()};

        std::uint64_t carry{0};
        for (std::size_t i{0}; i < n; ++i) {
            std::uint64_t high{0};
            std::uint64_t low{mul64_wide(a_span[i], b_val, high)};
            low += carry;
            if (low < carry) {
                ++high;
            }
            out_span[i] = low;
            carry = high;
        }
        if (carry > 0ULL) {
            out_span[n] = carry;
            res.m_size = static_cast<std::uint32_t>(n + 1U);
        } else {
            res.m_size = static_cast<std::uint32_t>(n);
        }
        res.m_sign = sign;
    }

    /// <summary>
    /// High-level Karatsuba multiplication wrapper with scratchpad/ScratchArena dispatch.
    /// Karatsuba 快速乘法演算法高階包裝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_karatsuba(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        const std::size_t n{std::max<std::size_t>(a.m_size, b.m_size)};
        if (n < KARATSUBA_THRESHOLD || a.m_size == 0U || b.m_size == 0U) {
            mul_schoolbook(res, a, b);
            return;
        }
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            mul_karatsuba(tmp, a, b);
            res = std::move(tmp);
            return;
        }

        const std::size_t total_len{a.m_size + b.m_size};
        res.resize(total_len, 0ULL);
        const std::size_t scratch_size{8U * n + 128U};

        if (std::is_constant_evaluated()) {
            if (scratch_size <= 1024U) {
                std::array<std::uint64_t, 1024> stack_scratch{};
                mul_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), stack_scratch);
            } else {
                std::vector<std::uint64_t> heap_scratch(scratch_size);
                mul_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), heap_scratch);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }

        if (scratch_size <= 1024U) {
            std::array<std::uint64_t, 1024> stack_scratch;
            mul_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), stack_scratch);
        } else {
            auto& arena{ScratchArena::instance()};
            const ScratchArena::Scope scope{arena};
            const std::span<std::uint64_t> scratch{arena.allocate(scratch_size)};
            mul_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), scratch);
        }

        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// High-level Toom-Cook 3-way multiplication wrapper utilizing ScratchArena.
    /// Toom-3 快速乘法演算法高階包裝。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_toom3(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        const std::size_t n{std::max<std::size_t>(a.m_size, b.m_size)};
        if (n < TOOM3_THRESHOLD || a.m_size == 0U || b.m_size == 0U) {
            mul_karatsuba(res, a, b);
            return;
        }
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            mul_toom3(tmp, a, b);
            res = std::move(tmp);
            return;
        }

        const std::size_t total_len{a.m_size + b.m_size};
        res.resize(total_len, 0ULL);
        const std::size_t scratch_size{48U * n + 2048U};

        if (std::is_constant_evaluated()) {
            if (scratch_size <= 2048U) {
                std::array<std::uint64_t, 2048> stack_scratch{};
                mul_toom3_raw(res.as_span(), a.as_span(), b.as_span(), stack_scratch);
            } else {
                std::vector<std::uint64_t> heap_scratch(scratch_size);
                mul_toom3_raw(res.as_span(), a.as_span(), b.as_span(), heap_scratch);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }

        if (scratch_size <= 2048U) {
            std::array<std::uint64_t, 2048> stack_scratch;
            mul_toom3_raw(res.as_span(), a.as_span(), b.as_span(), stack_scratch);
        } else {
            auto& arena{ScratchArena::instance()};
            const ScratchArena::Scope scope{arena};
            const std::span<std::uint64_t> scratch{arena.allocate(scratch_size)};
            mul_toom3_raw(res.as_span(), a.as_span(), b.as_span(), scratch);
        }

        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// Internal multiplication dispatcher selecting optimal tier based on operand sizes.
    /// 內部乘法派發函式，依據位數自動切換 Schoolbook、Block Karatsuba、Karatsuba 與 Toom-3。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_core(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        const std::size_t n{std::max<std::size_t>(a.m_size, b.m_size)};
        if (a.m_size < KARATSUBA_THRESHOLD || b.m_size < KARATSUBA_THRESHOLD) {
            mul_schoolbook(res, a, b);
        } else if (a.m_size >= 2U * b.m_size || b.m_size >= 2U * a.m_size) {
            if (&res == &a || &res == &b) {
                BigIntStorage tmp;
                mul_core(tmp, a, b);
                res = std::move(tmp);
                return;
            }
            res.resize(a.m_size + b.m_size, 0ULL);
            if (std::is_constant_evaluated()) {
                std::vector<std::uint64_t> heap_scratch(8U * n + 256U);
                mul_block_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), heap_scratch);
                res.m_sign = 1;
                res.normalize();
                return;
            }
            auto& arena{ScratchArena::instance()};
            const ScratchArena::Scope scope{arena};
            const std::span<std::uint64_t> scratch{arena.allocate(8U * n + 256U)};
            mul_block_karatsuba_raw(res.as_span(), a.as_span(), b.as_span(), scratch);
            res.m_sign = 1;
            res.normalize();
        } else if (n < TOOM3_THRESHOLD) {
            mul_karatsuba(res, a, b);
        } else {
            mul_toom3(res, a, b);
        }
    }

    /// <summary>
    /// Signed integer multiplication: res = a * b.
    /// 帶符號乘法：res = a * b，含零值快路徑與單肢段原生直通路徑。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto mul_signed(
        BigIntStorage& res,
        const BigIntStorage& a,
        const BigIntStorage& b) -> void
    {
        if (a.m_sign == 0 || b.m_sign == 0 || a.m_size == 0U || b.m_size == 0U) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        const std::int8_t res_sign{static_cast<std::int8_t>(a.m_sign * b.m_sign)};

        if (a.m_size == 1U && b.m_size == 1U) {
            std::uint64_t hi{0};
            const std::uint64_t lo{mul64_wide(a.as_span()[0], b.as_span()[0], hi)};
            res.reset_heap();
            res.capacity_span()[0] = lo;
            if (hi > 0ULL) {
                res.capacity_span()[1] = hi;
                res.m_size = 2;
            } else {
                res.m_size = 1;
            }
            res.m_sign = res_sign;
            return;
        }

        if (a.m_size == 1U) {
            mul_single_limb(res, b, a.as_span()[0], res_sign);
            return;
        }
        if (b.m_size == 1U) {
            mul_single_limb(res, a, b.as_span()[0], res_sign);
            return;
        }

        mul_core(res, a, b);
        res.m_sign = res_sign;
    }
};

} // namespace detail
} // namespace numeric

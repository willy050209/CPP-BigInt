// filepath: include/numeric/detail/StringConversion.hpp
/**
 * @file StringConversion.hpp
 * @brief High-performance arbitrary-precision Radix-10 string serialization and deserialization for CPP-BigInt.
 *        Features 2-digit LUT formatting, reciprocal Radix-10^19 division, divide-and-conquer formatting/parsing
 *        over std::span<char> buffers, declarative digit validation, and C++23 constexpr evaluation.
 *        高效能任意精度十進位字串序列化與反序列化模組。
 *        結合 2-Digit LUT 查表、10^19 乘法求逆除法、基於 std::span<char> 之分治格式轉換與 C++23 constexpr 解析。
 */
#pragma once

#include "Division.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace numeric {
namespace detail {

/// <summary>
/// Radix-10 string conversion and parsing core inheriting BigIntDivision.
/// BigInt 十進位字串轉換與解析核心類別，繼承自 BigIntDivision。
/// </summary>
struct BigIntStringConversion : public BigIntDivision {
    static constexpr std::size_t FROM_STRING_DC_THRESHOLD{10U};

    /// <summary>
    /// Returns a 200-byte 2-digit decimal lookup table resident in L1 cache.
    /// 回傳常駐 L1 快取之 200 位元組雙字元十進位查詢表。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto get_digit_pairs() noexcept -> std::string_view {
        return
            "00010203040506070809"
            "10111213141516171819"
            "20212223242526272829"
            "30313233343536373839"
            "40414243444546474849"
            "50515253545556575859"
            "60616263646566676869"
            "70717273747576777879"
            "80818283848586878889"
            "90919293949596979899";
    }

    /// <summary>
    /// Computes exact base-10 digit count of a 64-bit unsigned integer (1..20 digits) without division.
    /// 以零硬體除法二分樹精準計算 64 位元無符號整數之十進位位數 (1..20)。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20_FORCEINLINE auto digits10_u64(const std::uint64_t v) noexcept -> std::size_t {
        if (v < 100000000ULL) {
            if (v < 10000ULL) {
                if (v < 100ULL) return (v < 10ULL) ? 1U : 2U;
                return (v < 1000ULL) ? 3U : 4U;
            }
            if (v < 1000000ULL) return (v < 100000ULL) ? 5U : 6U;
            return (v < 10000000ULL) ? 7U : 8U;
        }
        if (v < 10000000000000000ULL) {
            if (v < 1000000000000ULL) {
                if (v < 10000000000ULL) return (v < 1000000000ULL) ? 9U : 10U;
                return (v < 100000000000ULL) ? 11U : 12U;
            }
            if (v < 100000000000000ULL) return (v < 10000000000000ULL) ? 13U : 14U;
            return (v < 1000000000000000ULL) ? 15U : 16U;
        }
        if (v < 100000000000000000ULL) return 17U;
        if (v < 1000000000000000000ULL) return 18U;
        if (v < 10000000000000000000ULL) return 19U;
        return 20U;
    }

    /// <summary>
    /// Formats the most-significant base-10^19 chunk backward into exact known digit slots.
    /// 格式化最高位十進位區塊：利用已知位數由尾向頭倒序填寫 2-Digit LUT。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE auto format_highest_chunk(
        const std::span<char> dst,
        std::uint64_t val,
        const std::size_t digits) noexcept -> void
    {
        assert(dst.size() >= digits);
        const std::string_view pairs{get_digit_pairs()};
        std::size_t pos{digits};
        while (val >= 100000000ULL) {
            const std::uint64_t q{val / 100000000ULL};
            auto v32{static_cast<std::uint32_t>(val - q * 100000000ULL)};
            val = q;
            for (std::int32_t k{0}; k < 4; ++k) {
                const std::uint32_t q32{v32 / 100U};
                const std::uint32_t rem{v32 - q32 * 100U};
                v32 = q32;
                pos -= 2U;
                dst[pos] = pairs[rem * 2U];
                dst[pos + 1U] = pairs[rem * 2U + 1U];
            }
        }
        auto v32{static_cast<std::uint32_t>(val)};
        while (v32 >= 100U) {
            const std::uint32_t q32{v32 / 100U};
            const std::uint32_t rem{v32 - q32 * 100U};
            v32 = q32;
            pos -= 2U;
            dst[pos] = pairs[rem * 2U];
            dst[pos + 1U] = pairs[rem * 2U + 1U];
        }
        if (v32 < 10U) {
            dst[--pos] = static_cast<char>('0' + v32);
        } else {
            pos -= 2U;
            dst[pos] = pairs[v32 * 2U];
            dst[pos + 1U] = pairs[v32 * 2U + 1U];
        }
        assert(pos == 0U && "format_highest_chunk failed to match exact digit count");
    }

    /// <summary>
    /// Formats a fixed-width 19-digit chunk using 9 LUT pairs plus 1 leading digit.
    /// 格式化固定 19 位十進位區塊：倒序寫入 9 組 LUT 雙字元加上最高位單字元。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE auto format_chunk_19_digits(
        const std::span<char> dst,
        std::uint64_t val) noexcept -> void
    {
        assert(dst.size() >= 19U);
        const std::string_view pairs{get_digit_pairs()};
        for (std::int32_t p{8}; p >= 0; --p) {
            const auto rem{static_cast<std::uint32_t>(val % 100ULL)};
            val /= 100ULL;
            const auto base_idx{static_cast<std::size_t>(1 + p * 2)};
            dst[base_idx] = pairs[rem * 2U];
            dst[base_idx + 1U] = pairs[rem * 2U + 1U];
        }
        dst[0] = static_cast<char>('0' + val);
    }

    /// <summary>
    /// Formats a 64-bit unsigned integer into decimal ASCII characters without null termination.
    /// 將 64 位元無符號整數格式化為十進位字元序列寫入 buf。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto format_uint64_to_buf(
        const std::span<char> buf,
        std::uint64_t val) noexcept -> std::size_t
    {
        if (val == 0ULL) {
            buf[0] = '0';
            return 1U;
        }
        std::array<char, 24> tmp{};
        std::size_t pos{0};
        while (val > 0ULL) {
            tmp[pos++] = static_cast<char>('0' + (val % 10ULL));
            val /= 10ULL;
        }
        for (std::size_t i{0}; i < pos; ++i) {
            buf[i] = tmp[pos - 1U - i];
        }
        return pos;
    }

    /// <summary>
    /// Thread-safe power-of-ten cache storing 10^(19 * 2^k) tables with cold-path lazy expansion.
    /// 線程安全且帶 [[unlikely]] 冷路徑擴展之 10^(19 * 2^k) 冪次快取表。
    /// </summary>
    class Pow10Cache final {
    public:
        static constexpr std::size_t PRECOMPUTED_LEVELS{7U}; // k = 0..6 (~10^1216)
        static constexpr std::size_t MAX_LEVELS{13U};        // Up to 77,824 digits
        std::array<BigIntStorage, MAX_LEVELS> table{};
        std::atomic<std::size_t> computed_levels{PRECOMPUTED_LEVELS};
        std::mutex mtx{};

        NUMERIC_NODISCARD static auto instance() -> Pow10Cache& {
            static Pow10Cache cache{};
            return cache;
        }

        NUMERIC_NODISCARD auto get_pow(const std::size_t k) -> const BigIntStorage& {
            assert(k < MAX_LEVELS);
            if (k >= computed_levels.load(std::memory_order_acquire)) [[unlikely]] {
                const std::lock_guard<std::mutex> lock{mtx};
                const std::size_t cur{computed_levels.load(std::memory_order_relaxed)};
                for (std::size_t i{cur}; i <= k && i < MAX_LEVELS; ++i) {
                    mul_core(table[i], table[i - 1U], table[i - 1U]);
                    computed_levels.store(i + 1U, std::memory_order_release);
                }
            }
            return table[k];
        }

    private:
        Pow10Cache() {
            table[0].set_uint64(10000000000000000000ULL, 1);
            for (std::size_t k{1U}; k < PRECOMPUTED_LEVELS; ++k) {
                mul_core(table[k], table[k - 1U], table[k - 1U]);
            }
        }
    };

    /// <summary>
    /// Dynamically computes 10^(19 * 2^k) when k exceeds cached levels.
    /// 當 k >= Pow10Cache::MAX_LEVELS 時的局部動態冪次求值。
    /// </summary>
    NUMERIC_NODISCARD static auto compute_pow10_dynamic(const std::size_t k) -> BigIntStorage {
        BigIntStorage cur{Pow10Cache::instance().get_pow(Pow10Cache::MAX_LEVELS - 1U)};
        for (std::size_t i{Pow10Cache::MAX_LEVELS - 1U}; i < k; ++i) {
            BigIntStorage nxt;
            mul_core(nxt, cur, cur);
            cur = std::move(nxt);
        }
        return cur;
    }

    /// <summary>
    /// Linearly accumulates 19-digit base chunks using in-place single-limb multiply-add.
    /// 線性累積解析 19-digit chunks span，採用原地單肢段乘加運算。
    /// </summary>
    NUMERIC_NODISCARD static auto parse_chunks_linear(const std::span<const std::uint64_t> chunks) -> BigIntStorage {
        const std::size_t count{chunks.size()};
        BigIntStorage cur;
        cur.set_uint64(chunks[count - 1U], 1);
        constexpr std::uint64_t RADIX10_19{10000000000000000000ULL};

        for (std::size_t i{count - 1U}; i > 0U; --i) {
            const std::uint64_t chunk_val{chunks[i - 1U]};
            if (cur.m_capacity < cur.m_size + 1U) {
                cur.reserve(cur.m_size + 2U);
            }
            std::uint64_t carry{chunk_val};
            const std::span<std::uint64_t> d{cur.as_span()};
            for (std::size_t j{0}; j < cur.m_size; ++j) {
                std::uint64_t hi{0};
                std::uint64_t lo{mul64_wide(d[j], RADIX10_19, hi)};
                lo += carry;
                if (lo < carry) ++hi;
                d[j] = lo;
                carry = hi;
            }
            if (carry > 0ULL) {
                cur.capacity_span()[cur.m_size] = carry;
                cur.m_size += 1U;
            }
        }
        cur.m_sign = 1;
        cur.normalize();
        return cur;
    }

    /// <summary>
    /// Divide-and-conquer binary-tree accumulation over a span of 19-digit chunks.
    /// 分治平衡樹解析 19-digit chunks span。
    /// </summary>
    NUMERIC_NODISCARD static auto parse_chunks_dc(const std::span<const std::uint64_t> chunks) -> BigIntStorage {
        const std::size_t count{chunks.size()};
        if (count <= FROM_STRING_DC_THRESHOLD) {
            return parse_chunks_linear(chunks);
        }

        std::size_t k{0};
        while ((std::size_t{1U} << (k + 1U)) < count) {
            ++k;
        }
        const std::size_t split{std::size_t{1U} << k};

        const BigIntStorage low{parse_chunks_dc(chunks.subspan(0U, split))};
        const BigIntStorage high{parse_chunks_dc(chunks.subspan(split))};

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr{nullptr};
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        BigIntStorage high_scaled;
        mul_core(high_scaled, high, *pow_ptr);

        BigIntStorage res;
        add_signed(res, high_scaled, low);
        return res;
    }

    /// <summary>
    /// Constexpr compile-time decimal string parser using stateless Horner accumulation.
    /// 編譯期常數求值專用十進位字串解析函式，採用無靜態局部變數之 Horner 演算法。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto from_string_constexpr(
        BigIntStorage& res,
        const numeric::string_view sv) -> void
    {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty bigint string"));
        }
        std::size_t idx{0};
        std::int8_t sign{1};
        if (sv[0] == '-') {
            sign = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no digits in bigint string"));
        }
        const bool all_digits{std::ranges::all_of(sv.substr(idx), [](const char ch) noexcept -> bool {
            return ch >= '0' && ch <= '9';
        })};
        if (!all_digits) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
        }
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }

        const std::size_t total_digits{sv.size() - idx};
        if (total_digits <= 19U) {
            std::uint64_t val{0};
            for (const char ch : sv.substr(idx)) {
                val = val * 10ULL + static_cast<std::uint64_t>(ch - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        BigIntStorage cur;
        cur.reset_heap();
        cur.m_size = 0;
        cur.m_sign = 0;

        while (idx < sv.size()) {
            const std::size_t take{(sv.size() - idx >= 9U) ? 9U : (sv.size() - idx)};
            std::uint64_t chunk_val{0};
            std::uint64_t multiplier{1ULL};
            for (std::size_t i{0}; i < take; ++i) {
                chunk_val = chunk_val * 10ULL + static_cast<std::uint64_t>(sv[idx + i] - '0');
                multiplier *= 10ULL;
            }
            idx += take;

            if (cur.m_size > 0U) {
                BigIntStorage next_cur;
                mul_single_limb(next_cur, cur, multiplier, 1);
                BigIntStorage chunk_st;
                chunk_st.set_uint64(chunk_val, 1);
                add_signed(cur, next_cur, chunk_st);
            } else {
                cur.set_uint64(chunk_val, 1);
            }
        }
        cur.m_sign = sign;
        cur.normalize();
        res = std::move(cur);
    }

    /// <summary>
    /// High-speed runtime decimal string parser using divide-and-conquer and Pow10Cache.
    /// 執行期高效十進位字串解析（分治法 + Pow10Cache 冪次表快取）。
    /// </summary>
    static auto from_string_runtime(BigIntStorage& res, const numeric::string_view sv) -> void {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty bigint string"));
        }
        std::size_t idx{0};
        std::int8_t sign{1};
        if (sv[0] == '-') {
            sign = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no digits in bigint string"));
        }

        const bool all_digits{std::ranges::all_of(sv.substr(idx), [](const char ch) noexcept -> bool {
            return ch >= '0' && ch <= '9';
        })};
        if (!all_digits) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
        }

        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }

        const std::size_t total_digits{sv.size() - idx};
        if (total_digits <= 19U) {
            std::uint64_t val{0};
            for (const char ch : sv.substr(idx)) {
                val = val * 10ULL + static_cast<std::uint64_t>(ch - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        std::vector<std::uint64_t> chunks{};
        chunks.reserve((total_digits + 18U) / 19U);

        std::size_t curr_end{sv.size()};
        while (curr_end > idx) {
            const std::size_t chunk_start{(curr_end - idx >= 19U) ? (curr_end - 19U) : idx};
            std::uint64_t val{0};
            for (std::size_t i{chunk_start}; i < curr_end; ++i) {
                val = val * 10ULL + static_cast<std::uint64_t>(sv[i] - '0');
            }
            chunks.push_back(val);
            curr_end = chunk_start;
        }

        if (chunks.size() <= FROM_STRING_DC_THRESHOLD) {
            res = parse_chunks_linear(chunks);
            res.m_sign = sign;
            res.normalize();
            return;
        }

        res = parse_chunks_dc(chunks);
        res.m_sign = sign;
        res.normalize();
    }

    /// <summary>
    /// Unified decimal string parser dispatching between constexpr and runtime implementations.
    /// 字串解析統一入口：於編譯期與執行期間自動分派最佳路徑。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 auto from_string(
        BigIntStorage& res,
        const numeric::string_view sv) -> void
    {
        if (std::is_constant_evaluated()) {
            from_string_constexpr(res, sv);
            return;
        }
        from_string_runtime(res, sv);
    }

    /// <summary>
    /// Basecase chunk extraction via 10^19 reciprocal division into out_chunks span.
    /// 基底情況：透過 10^19 乘法求逆除法將整數萃取至 out_chunks span（不足高位補 0）。
    /// </summary>
    static auto extract_chunks_basecase(
        const BigIntStorage& val,
        const std::span<std::uint64_t> out_chunks) -> void
    {
        BigIntStorage::zero_limbs(out_chunks);
        if (val.m_size == 0U || val.m_sign == 0) {
            return;
        }

        std::array<std::uint64_t, 128> stack_limbs{};
        std::vector<std::uint64_t> heap_limbs{};
        std::span<std::uint64_t> work_limbs{stack_limbs};
        if (val.m_size > 128U) {
            heap_limbs.resize(val.m_size);
            work_limbs = std::span<std::uint64_t>{heap_limbs};
        }
        BigIntStorage::copy_limbs(work_limbs, val.as_span());
        std::size_t work_size{val.m_size};
        std::size_t idx{0};

        while (work_size > 0U && idx < out_chunks.size()) {
            std::uint64_t rem{0};
            for (std::size_t i{work_size}; i > 0U; --i) {
                std::uint64_t next_rem{0};
                work_limbs[i - 1U] = div_recip_radix10_19(rem, work_limbs[i - 1U], next_rem);
                rem = next_rem;
            }
            while (work_size > 0U && work_limbs[work_size - 1U] == 0ULL) {
                --work_size;
            }
            out_chunks[idx++] = rem;
        }
    }

    /// <summary>
    /// Divide-and-conquer zero-padded formatting writing exactly num_chunks * 19 characters.
    /// 分治固定位數格式化：保證精確寫入 num_chunks * 19 個字元（高位以 '0' 補足）。
    /// </summary>
    static auto format_padded_dc(
        const std::span<char> dst,
        const BigIntStorage& val,
        const std::size_t num_chunks) -> void
    {
        if (num_chunks <= 16U) {
            std::array<std::uint64_t, 16> chunks{};
            extract_chunks_basecase(val, std::span<std::uint64_t>{chunks}.subspan(0U, num_chunks));
            for (std::size_t i{num_chunks}; i > 0U; --i) {
                const std::size_t out_offset{(num_chunks - i) * 19U};
                format_chunk_19_digits(dst.subspan(out_offset, 19U), chunks[i - 1U]);
            }
            return;
        }

        std::size_t k{0};
        while ((std::size_t{1U} << (k + 1U)) < num_chunks) {
            ++k;
        }
        const std::size_t split{std::size_t{1U} << k};

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr{nullptr};
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        BigIntStorage q;
        BigIntStorage r;
        div_qr(q, r, val, *pow_ptr);

        const std::size_t high_chunks{num_chunks - split};
        format_padded_dc(dst, q, high_chunks);
        format_padded_dc(dst.subspan(high_chunks * 19U), r, split);
    }

    /// <summary>
    /// Divide-and-conquer unpadded formatting omitting leading zeros. Returns characters written.
    /// 分治非固定位數格式化：最頂部數值無前導 0 寫入，回傳實際寫入字元總長。
    /// </summary>
    NUMERIC_NODISCARD static auto format_unpadded_dc(
        const std::span<char> dst,
        const BigIntStorage& val) -> std::size_t
    {
        if (val.m_size == 0U || val.m_sign == 0) {
            dst[0] = '0';
            return 1U;
        }

        if (val.m_size <= 16U) {
            std::array<std::uint64_t, 32> stack_limbs{};
            BigIntStorage::copy_limbs(stack_limbs, val.as_span());
            std::size_t work_size{val.m_size};

            std::array<std::uint64_t, 32> chunks{};
            std::size_t chunk_count{0};

            while (work_size > 0U) {
                std::uint64_t rem{0};
                for (std::size_t i{work_size}; i > 0U; --i) {
                    std::uint64_t next_rem{0};
                    stack_limbs[i - 1U] = div_recip_radix10_19(rem, stack_limbs[i - 1U], next_rem);
                    rem = next_rem;
                }
                while (work_size > 0U && stack_limbs[work_size - 1U] == 0ULL) {
                    --work_size;
                }
                chunks[chunk_count++] = rem;
            }

            if (chunk_count == 0U) {
                dst[0] = '0';
                return 1U;
            }

            const std::size_t top_digits{digits10_u64(chunks[chunk_count - 1U])};
            format_highest_chunk(dst, chunks[chunk_count - 1U], top_digits);

            for (std::size_t i{chunk_count - 1U}; i > 0U; --i) {
                const std::size_t offset{top_digits + (chunk_count - 1U - i) * 19U};
                format_chunk_19_digits(dst.subspan(offset, 19U), chunks[i - 1U]);
            }
            return (chunk_count - 1U) * 19U + top_digits;
        }

        const std::size_t est_chunks{(val.m_size * 64U + 62U) / 63U};
        std::size_t k{0};
        while ((std::size_t{1U} << (k + 1U)) < est_chunks) {
            ++k;
        }

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr{nullptr};
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        while (k > 0U && compare_unsigned(val.as_span(), pow_ptr->as_span()) < 0) {
            --k;
            if (k < Pow10Cache::MAX_LEVELS) {
                pow_ptr = &Pow10Cache::instance().get_pow(k);
            } else {
                dynamic_pow = compute_pow10_dynamic(k);
                pow_ptr = &dynamic_pow;
            }
        }

        const std::size_t split{std::size_t{1U} << k};
        BigIntStorage q;
        BigIntStorage r;
        div_qr(q, r, val, *pow_ptr);

        if (q.m_size == 0U || q.m_sign == 0) {
            return format_unpadded_dc(dst, r);
        }

        const std::size_t q_len{format_unpadded_dc(dst, q)};
        format_padded_dc(dst.subspan(q_len), r, split);
        return q_len + split * 19U;
    }

    /// <summary>
    /// Converts BigIntStorage into a base-10 decimal std::string.
    /// 將 BigIntStorage 轉換為十進位字串，涵蓋單肢快路徑、棧上無配置與分治大數路徑。
    /// </summary>
    NUMERIC_NODISCARD static auto to_string(const BigIntStorage& a) -> std::string {
        if (a.m_size == 0U || a.m_sign == 0) {
            return "0";
        }

        if (a.m_size == 1U) {
            const std::uint64_t val{a.as_span()[0]};
            if (val == 0ULL) return "0";
            const std::size_t digits{digits10_u64(val)};
            std::array<char, 32> stack_buf{};
            std::size_t offset{0};
            if (a.m_sign < 0) {
                stack_buf[offset++] = '-';
            }
            format_highest_chunk(std::span<char>{stack_buf}.subspan(offset), val, digits);
            return std::string{stack_buf.data(), offset + digits};
        }

        if (a.m_size <= 16U) {
            std::array<char, 512> stack_buf{};
            std::size_t offset{0};
            if (a.m_sign < 0) {
                stack_buf[offset++] = '-';
            }
            const std::size_t digits_written{format_unpadded_dc(std::span<char>{stack_buf}.subspan(offset), a)};
            return std::string{stack_buf.data(), offset + digits_written};
        }

        const std::size_t max_digits{a.m_size * 20U + 24U};
        std::string result(max_digits, '\0');
        const std::span<char> out_span{result};
        std::size_t offset{0};
        if (a.m_sign < 0) {
            out_span[offset++] = '-';
        }
        const std::size_t digits_written{format_unpadded_dc(out_span.subspan(offset), a)};
        result.resize(offset + digits_written);
        return result;
    }
};

} // namespace detail
} // namespace numeric

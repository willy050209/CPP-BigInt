#pragma once

// StringConversion.hpp
// High-performance arbitrary-precision Radix-10 string serialization and deserialization for CPP-BigInt.
// Features 2-digit LUT formatting, Reciprocal Radix-10^19 division, Divide-and-Conquer string conversion,
// and compile-time constexpr Horner evaluation.
// Part of the numeric::detail modular core.

#include "Division.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <cassert>
#include <stdexcept>
#include <cstdint>
#include <cstddef>

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 十進位字串轉換類別，繼承自 BigIntDivision。
/// 支援分治十進位轉換、Pow10Cache 快取、2-Digit LUT、以及 C++20 constexpr 字串解析。
/// </summary>
struct BigIntStringConversion : public BigIntDivision {
    static constexpr size_t FROM_STRING_DC_THRESHOLD = 10;

    /// <summary>
    /// 2-Digit 快速十進位查詢表（200 位元組，長駐 L1 Cache）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE const char* get_digit_pairs() noexcept {
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
    /// 支援完整 64 位元無符號整數（最大 18446744073709551615，共 20 位）之精準十進位位數判定。
    /// 採零硬體除法二分判定分支，單一週期等級解析，杜絕任何緩衝區溢位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE size_t digits10_u64(uint64_t v) noexcept {
        if (v < 100000000ULL) { // < 10^8
            if (v < 10000ULL) { // < 10^4
                if (v < 100ULL) return (v < 10ULL) ? 1 : 2;
                else return (v < 1000ULL) ? 3 : 4;
            } else {
                if (v < 1000000ULL) return (v < 100000ULL) ? 5 : 6;
                else return (v < 10000000ULL) ? 7 : 8;
            }
        } else if (v < 10000000000000000ULL) { // < 10^16
            if (v < 1000000000000ULL) { // < 10^12
                if (v < 10000000000ULL) return (v < 1000000000ULL) ? 9 : 10;
                else return (v < 100000000000ULL) ? 11 : 12;
            } else {
                if (v < 100000000000000ULL) return (v < 10000000000000ULL) ? 13 : 14;
                else return (v < 1000000000000000ULL) ? 15 : 16;
            }
        } else { // >= 10^16
            if (v < 100000000000000000ULL) return 17;
            if (v < 1000000000000000000ULL) return 18;
            if (v < 10000000000000000000ULL) return 19;
            return 20;
        }
    }

    /// <summary>
    /// 格式化最高位 chunk：直接利用已知的 top_digits 由尾向頭倒序填寫。
    /// 採 10^8 區塊分割與純 32 位元倒序倒數乘法，徹底消除 64 位元硬體除法延遲。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE void format_highest_chunk(
        char* dst, uint64_t val, size_t digits) noexcept
    {
        const char* pairs = get_digit_pairs();
        int pos = static_cast<int>(digits);
        while (val >= 100000000ULL) {
            uint64_t q = val / 100000000ULL;
            uint32_t v32 = static_cast<uint32_t>(val - q * 100000000ULL);
            val = q;
            for (int k = 0; k < 4; ++k) {
                uint32_t q32 = v32 / 100;
                uint32_t rem = v32 - q32 * 100;
                v32 = q32;
                pos -= 2;
                dst[pos]     = pairs[rem * 2];
                dst[pos + 1] = pairs[rem * 2 + 1];
            }
        }
        uint32_t v32 = static_cast<uint32_t>(val);
        while (v32 >= 100) {
            uint32_t q32 = v32 / 100;
            uint32_t rem = v32 - q32 * 100;
            v32 = q32;
            pos -= 2;
            dst[pos]     = pairs[rem * 2];
            dst[pos + 1] = pairs[rem * 2 + 1];
        }
        if (v32 < 10) {
            dst[--pos] = static_cast<char>('0' + v32);
        } else {
            pos -= 2;
            dst[pos]     = pairs[v32 * 2];
            dst[pos + 1] = pairs[v32 * 2 + 1];
        }
        assert(pos == 0 && "format_highest_chunk failed to match exact digit count");
    }

    /// <summary>
    /// 格式化中間 19 位 fixed-width chunk：倒序逆向填充 9 組 LUT 雙字元加上 1 個最高位單字元。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE void format_chunk_19_digits(
        char* dst, uint64_t val) noexcept
    {
        const char* pairs = get_digit_pairs();
        for (int p = 8; p >= 0; --p) {
            uint32_t rem = static_cast<uint32_t>(val % 100);
            val /= 100;
            dst[1 + p * 2]     = pairs[rem * 2];
            dst[1 + p * 2 + 1] = pairs[rem * 2 + 1];
        }
        dst[0] = static_cast<char>('0' + val);
    }

    /// <summary>
    /// 將 64 位元無符號整數格式化為十進位字元陣列寫入 buf（不含 null 結尾），回傳字元長度。
    /// 棧上無配置零開銷輔助函式。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 size_t format_uint64_to_buf(char* buf, uint64_t val) noexcept {
        if (val == 0) {
            buf[0] = '0';
            return 1;
        }
        char tmp[24];
        size_t pos = 0;
        while (val > 0) {
            tmp[pos++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
        for (size_t i = 0; i < pos; ++i) {
            buf[i] = tmp[pos - 1 - i];
        }
        return pos;
    }

    /// <summary>
    /// 線程安全且帶 [[unlikely]] 冷路徑優化之 10^(19 * 2^k) 冪次快取表。
    /// 預先計算前 7 階 (k = 0..6，涵蓋至 10^1216)，冷啟動小於 1 微秒。
    /// </summary>
    class Pow10Cache {
    public:
        static constexpr size_t PRECOMPUTED_LEVELS = 7; // k = 0..6 (涵蓋至 10^1216)
        static constexpr size_t MAX_LEVELS = 13;        // 支援至 77,824 位
        BigIntStorage table[MAX_LEVELS];
        std::atomic<size_t> computed_levels{PRECOMPUTED_LEVELS};
        std::mutex mtx;

        static Pow10Cache& instance() {
            static Pow10Cache cache;
            return cache;
        }

        const BigIntStorage& get_pow(size_t k) {
            assert(k < MAX_LEVELS);
            if (k >= computed_levels.load(std::memory_order_acquire))
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
                [[unlikely]]
#endif
            {
                std::lock_guard<std::mutex> lock(mtx);
                size_t cur = computed_levels.load(std::memory_order_relaxed);
                for (size_t i = cur; i <= k && i < MAX_LEVELS; ++i) {
                    mul_core(table[i], table[i - 1], table[i - 1]);
                    computed_levels.store(i + 1, std::memory_order_release);
                }
            }
            return table[k];
        }

    private:
        Pow10Cache() {
            table[0].set_uint64(10000000000000000000ULL, 1);
            for (size_t k = 1; k < PRECOMPUTED_LEVELS; ++k) {
                mul_core(table[k], table[k - 1], table[k - 1]);
            }
        }
    };

    /// <summary>
    /// 當 k >= Pow10Cache::MAX_LEVELS 時的局部動態冪次求值。
    /// </summary>
    static BigIntStorage compute_pow10_dynamic(size_t k) {
        BigIntStorage cur = Pow10Cache::instance().get_pow(Pow10Cache::MAX_LEVELS - 1);
        for (size_t i = Pow10Cache::MAX_LEVELS - 1; i < k; ++i) {
            BigIntStorage nxt;
            mul_core(nxt, cur, cur);
            cur = std::move(nxt);
        }
        return cur;
    }

    /// <summary>
    /// 線性累積解析 19-digit chunks 陣列，採用原地單 limb 乘加運算，零臨時配置。
    /// </summary>
    static BigIntStorage parse_chunks_linear(const uint64_t* chunks, size_t count) {
        BigIntStorage cur;
        cur.set_uint64(chunks[count - 1], 1);
        constexpr uint64_t RADIX10_19 = 10000000000000000000ULL;
        for (size_t i = count - 1; i > 0; --i) {
            uint64_t chunk_val = chunks[i - 1];
            if (cur.m_capacity < cur.m_size + 1) {
                cur.reserve(cur.m_size + 2);
            }
            uint64_t carry = chunk_val;
            for (size_t j = 0; j < cur.m_size; ++j) {
                uint64_t hi = 0;
                uint64_t lo = mul64_wide(cur.data()[j], RADIX10_19, hi);
                lo += carry;
                if (lo < carry) ++hi;
                cur.data()[j] = lo;
                carry = hi;
            }
            if (carry > 0) {
                cur.data()[cur.m_size] = carry;
                cur.m_size += 1;
            }
        }
        cur.m_sign = 1;
        cur.normalize();
        return cur;
    }

    /// <summary>
    /// 分治解析 19-digit chunks 陣列 [start, end)。
    /// </summary>
    static BigIntStorage parse_chunks_dc(const uint64_t* chunks, size_t start, size_t end) {
        size_t count = end - start;
        if (count <= FROM_STRING_DC_THRESHOLD) {
            return parse_chunks_linear(chunks + start, count);
        }

        // 尋找切分點：最大的 2^k 使得 2^k < count
        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < count) {
            ++k;
        }
        size_t split = static_cast<size_t>(1) << k;

        BigIntStorage low = parse_chunks_dc(chunks, start, start + split);
        BigIntStorage high = parse_chunks_dc(chunks, start + split, end);

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
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
    /// C++20 constexpr 編譯期常數求值專用字串解析函式。
    /// 採用無靜態局部變數之 Horner 演算法，保證常數求值完全相容。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void from_string_constexpr(BigIntStorage& res, numeric::string_view sv) {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty bigint string"));
        }
        size_t idx = 0;
        int8_t sign = 1;
        if (sv[0] == '-') {
            sign = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no digits in bigint string"));
        }
        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] < '0' || sv[i] > '9') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
            }
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

        size_t total_digits = sv.size() - idx;
        if (total_digits <= 19) {
            uint64_t val = 0;
            for (size_t i = idx; i < sv.size(); ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        BigIntStorage cur;
        cur.reset_heap();
        cur.m_size = 0;
        cur.m_sign = 0;

        while (idx < sv.size()) {
            size_t take = (sv.size() - idx >= 9) ? 9 : (sv.size() - idx);
            uint64_t chunk_val = 0;
            uint64_t multiplier = 1;
            for (size_t i = 0; i < take; ++i) {
                chunk_val = chunk_val * 10 + static_cast<uint64_t>(sv[idx + i] - '0');
                multiplier *= 10;
            }
            idx += take;

            if (cur.m_size > 0) {
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
    /// 執行期高效十進位字串解析（分治法 + Pow10Cache 冪次表快取）。
    /// </summary>
    static void from_string_runtime(BigIntStorage& res, numeric::string_view sv) {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty bigint string"));
        }
        size_t idx = 0;
        int8_t sign = 1;
        if (sv[0] == '-') {
            sign = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no digits in bigint string"));
        }

        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] < '0' || sv[i] > '9') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
            }
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

        size_t total_digits = sv.size() - idx;
        if (total_digits <= 19) {
            uint64_t val = 0;
            for (size_t i = idx; i < sv.size(); ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        std::vector<uint64_t> chunks;
        chunks.reserve((total_digits + 18) / 19);

        size_t curr_end = sv.size();
        while (curr_end > idx) {
            size_t chunk_start = (curr_end - idx >= 19) ? (curr_end - 19) : idx;
            uint64_t val = 0;
            for (size_t i = chunk_start; i < curr_end; ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            chunks.push_back(val);
            curr_end = chunk_start;
        }

        if (chunks.size() <= FROM_STRING_DC_THRESHOLD) {
            res = parse_chunks_linear(chunks.data(), chunks.size());
            res.m_sign = sign;
            res.normalize();
            return;
        }

        res = parse_chunks_dc(chunks.data(), 0, chunks.size());
        res.m_sign = sign;
        res.normalize();
    }

    /// <summary>
    /// 字串解析統一入口：編譯期常數求值時派發至 from_string_constexpr，執行期派發至 from_string_runtime。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void from_string(BigIntStorage& res, numeric::string_view sv) {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            from_string_constexpr(res, sv);
            return;
        }
#endif
        from_string_runtime(res, sv);
    }

    /// <summary>
    /// 基底情況：將任意小於 10^(19 * count) 之大數透過 10^19 乘法求逆除法提取為 count 個 chunks。
    /// 不足 count 個 chunk 者高位自動補 0。
    /// </summary>
    static void extract_chunks_basecase(const BigIntStorage& val, uint64_t* out_chunks, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            out_chunks[i] = 0;
        }
        if (val.m_size == 0 || val.m_sign == 0) {
            return;
        }

        uint64_t stack_limbs[128];
        std::vector<uint64_t> heap_limbs;
        uint64_t* work_limbs = stack_limbs;
        if (val.m_size > 128) {
            heap_limbs.resize(val.m_size);
            work_limbs = heap_limbs.data();
        }
        std::copy_n(val.data(), val.m_size, work_limbs);
        size_t work_size = val.m_size;
        size_t idx = 0;

        while (work_size > 0 && idx < count) {
            uint64_t rem = 0;
            for (size_t i = work_size; i > 0; --i) {
                uint64_t next_rem = 0;
                work_limbs[i - 1] = div_recip_radix10_19(rem, work_limbs[i - 1], next_rem);
                rem = next_rem;
            }
            while (work_size > 0 && work_limbs[work_size - 1] == 0) {
                --work_size;
            }
            out_chunks[idx++] = rem;
        }
    }

    /// <summary>
    /// 分治固定位元格式化：保證剛好寫入 num_chunks * 19 個字元（高位以 '0' 補足）。
    /// </summary>
    static void format_padded_dc(char* dst, const BigIntStorage& val, size_t num_chunks) {
        if (num_chunks <= 16) {
            uint64_t chunks[16];
            extract_chunks_basecase(val, chunks, num_chunks);
            for (size_t i = num_chunks; i > 0; --i) {
                format_chunk_19_digits(dst, chunks[i - 1]);
                dst += 19;
            }
            return;
        }

        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < num_chunks) {
            ++k;
        }
        size_t split = static_cast<size_t>(1) << k;

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        BigIntStorage q, r;
        div_qr(q, r, val, *pow_ptr);

        size_t high_chunks = num_chunks - split;
        format_padded_dc(dst, q, high_chunks);
        format_padded_dc(dst + high_chunks * 19, r, split);
    }

    /// <summary>
    /// 分治非固定位元格式化：最頂部數值無前導 0 寫入，回傳實際寫入字元總長。
    /// </summary>
    static size_t format_unpadded_dc(char* dst, const BigIntStorage& val) {
        if (val.m_size == 0 || val.m_sign == 0) {
            dst[0] = '0';
            return 1;
        }

        // 當數值小於等於 16 chunks (~304 十進位位元，約 1010 bits) 時採用 basecase 展開
        if (val.m_size <= 16) {
            uint64_t stack_limbs[32];
            std::copy_n(val.data(), val.m_size, stack_limbs);
            size_t work_size = val.m_size;

            uint64_t chunks[32];
            size_t chunk_count = 0;

            while (work_size > 0) {
                uint64_t rem = 0;
                for (size_t i = work_size; i > 0; --i) {
                    uint64_t next_rem = 0;
                    stack_limbs[i - 1] = div_recip_radix10_19(rem, stack_limbs[i - 1], next_rem);
                    rem = next_rem;
                }
                while (work_size > 0 && stack_limbs[work_size - 1] == 0) {
                    --work_size;
                }
                chunks[chunk_count++] = rem;
            }

            if (chunk_count == 0) {
                dst[0] = '0';
                return 1;
            }

            size_t top_digits = digits10_u64(chunks[chunk_count - 1]);
            format_highest_chunk(dst, chunks[chunk_count - 1], top_digits);
            char* out_ptr = dst + top_digits;

            for (size_t i = chunk_count - 1; i > 0; --i) {
                format_chunk_19_digits(out_ptr, chunks[i - 1]);
                out_ptr += 19;
            }
            return (chunk_count - 1) * 19 + top_digits;
        }

        // 大數分治：依據 val 之 bit 長度估計所需總 chunks，尋找最佳 split = 2^k
        // 1 chunk = 10^19 ≈ 2^63.11 bits => chunks ≈ val.m_size * 64 / 63
        size_t est_chunks = (val.m_size * 64 + 62) / 63;
        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < est_chunks) {
            ++k;
        }

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        // 若估計的 2^k 超過或等於 val，則降一級
        while (k > 0 && compare_unsigned(val.data(), val.m_size, pow_ptr->data(), pow_ptr->m_size) < 0) {
            --k;
            if (k < Pow10Cache::MAX_LEVELS) {
                pow_ptr = &Pow10Cache::instance().get_pow(k);
            } else {
                dynamic_pow = compute_pow10_dynamic(k);
                pow_ptr = &dynamic_pow;
            }
        }

        size_t split = static_cast<size_t>(1) << k;
        BigIntStorage q, r;
        div_qr(q, r, val, *pow_ptr);

        if (q.m_size == 0 || q.m_sign == 0) {
            return format_unpadded_dc(dst, r);
        }

        size_t q_len = format_unpadded_dc(dst, q);
        format_padded_dc(dst + q_len, r, split);
        return q_len + split * 19;
    }

    /// <summary>
    /// 將 BigIntStorage 轉換為 Radix-10 十進位字串。
    /// 小於 1000 位元採高效 10^19 Reciprocal Division；大數自動切換至 Divide-and-Conquer Radix Conversion。
    /// </summary>
    static std::string to_string(const BigIntStorage& a) {
        if (a.m_size == 0 || a.m_sign == 0) {
            return "0";
        }

        // 單 limb 快速超短格式化路徑
        if (a.m_size == 1) {
            uint64_t val = a.data()[0];
            if (val == 0) return "0";
            size_t digits = digits10_u64(val);
            char stack_buf[32];
            char* ptr = stack_buf;
            if (a.m_sign < 0) *ptr++ = '-';
            format_highest_chunk(ptr, val, digits);
            size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits;
            return std::string(stack_buf, total_len);
        }

        // 小中型大數 (<= 16 limbs, 約 1024 bits)：棧上緩衝區零初步堆積配置
        if (a.m_size <= 16) {
            char stack_buf[512];
            char* out_ptr = stack_buf;
            if (a.m_sign < 0) {
                *out_ptr++ = '-';
            }
            size_t digits_written = format_unpadded_dc(out_ptr, a);
            size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits_written;
            return std::string(stack_buf, total_len);
        }

        // 大型大數 (> 16 limbs)：預估容量並以分治法格式化
        size_t max_digits = a.m_size * 20 + 24;
        std::string result;
        result.resize(max_digits);

        char* out_ptr = &result[0];
        if (a.m_sign < 0) {
            *out_ptr++ = '-';
        }

        size_t digits_written = format_unpadded_dc(out_ptr, a);
        size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits_written;
        result.resize(total_len);
        return result;
    }
};

} // namespace detail
} // namespace numeric

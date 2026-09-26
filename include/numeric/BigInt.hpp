#pragma once

// BigInt.hpp
// Arbitrary-precision integer facade class for CPP-BigInt.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "Config.hpp"
#include "detail/BigIntCore.hpp"
#include <cstdint>
#include <string>
#include <iostream>
#include <functional>
#include <type_traits>
#include <stdexcept>
#include <limits>
#include <cstdlib>
#include <bitset>

namespace numeric {

class bigint;

namespace detail {

    /// <summary>
    /// 常數輔助結構，支援以常數屬性或函式呼叫方式取得 bigint 常數（如 bigint::zero 與 bigint::zero()）。
    /// </summary>
    struct BigIntConstantProxy {
        int64_t value;

        /// <summary>
        /// 建構常數代理物件。
        /// </summary>
        /// <param name="v">整數值</param>
        constexpr explicit BigIntConstantProxy(int64_t v) noexcept : value(v) {}

        /// <summary>
        /// 隱式轉換至 bigint。
        /// </summary>
        /// <returns>對應之 bigint 實例</returns>
        NUMERIC_CONSTEXPR_20 operator bigint() const;

        /// <summary>
        /// 函式呼叫運算子，傳回對應之 bigint。
        /// </summary>
        /// <returns>對應之 bigint 實例</returns>
        NUMERIC_CONSTEXPR_20 bigint operator()() const;

        template <typename T>
        friend NUMERIC_CONSTEXPR_20 bool operator==(BigIntConstantProxy p, const T& other);

        template <typename T>
        friend NUMERIC_CONSTEXPR_20 bool operator==(const T& other, BigIntConstantProxy p);

        template <typename T>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(BigIntConstantProxy p, const T& other);

        template <typename T>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(const T& other, BigIntConstantProxy p);
    };

    /// <summary>
    /// BigInt 常數模板基底結構，確保在 C++11/C++14 header-only 環境下具備弱符號鏈結與外部定義。
    /// </summary>
    template <typename T = void>
    struct BigIntConstants {
        static constexpr BigIntConstantProxy zero{0};
        static constexpr BigIntConstantProxy one{1};
    };

#if (NUMERIC_CPLUSPLUS < NUMERIC_CXX_17)
    template <typename T>
    constexpr BigIntConstantProxy BigIntConstants<T>::zero;
    template <typename T>
    constexpr BigIntConstantProxy BigIntConstants<T>::one;
#endif

} // namespace detail

/// <summary>
/// 任意精度整數類別，具備 256-bit Small Buffer Optimization (SBO) 與全套運算子重載。
/// </summary>
class bigint : public detail::BigIntConstants<> {
private:
    detail::BigIntStorage m_storage;

public:


    /// <summary>
    /// 預設建構子：初始化數值為 0。
    /// </summary>
    NUMERIC_CONSTEXPR_20 bigint() noexcept = default;

    /// <summary>
    /// 複製建構子。
    /// </summary>
    /// <param name="other">來源 bigint</param>
    NUMERIC_CONSTEXPR_20 bigint(const bigint& other) = default;

    /// <summary>
    /// 移動建構子。
    /// </summary>
    /// <param name="other">來源 bigint（右值）</param>
    NUMERIC_CONSTEXPR_20 bigint(bigint&& other) noexcept = default;

    /// <summary>
    /// 複製賦值運算子。
    /// </summary>
    /// <param name="other">來源 bigint</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator=(const bigint& other) = default;

    /// <summary>
    /// 移動賦值運算子。
    /// </summary>
    /// <param name="other">來源 bigint（右值）</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator=(bigint&& other) noexcept = default;

    /// <summary>
    /// 解構子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~bigint() = default;

    /// <summary>
    /// 自內部 BigIntStorage 建立 bigint。
    /// </summary>
    /// <param name="storage">來源儲存層物件</param>
    explicit NUMERIC_CONSTEXPR_20 bigint(detail::BigIntStorage storage) noexcept
        : m_storage(std::move(storage)) {}

    /// <summary>
    /// 自布林值建構：true 為 1，false 為 0。
    /// </summary>
    /// <param name="b">布林值</param>
    NUMERIC_CONSTEXPR_20 bigint(bool b) noexcept {
        m_storage.set_uint64(b ? 1 : 0, b ? 1 : 0);
    }

    /// <summary>
    /// 自 8 位元有符號整數建構。
    /// </summary>
    /// <param name="v">8 位元整數</param>
    NUMERIC_CONSTEXPR_20 bigint(int8_t v) noexcept {
        if (v < 0) {
            m_storage.set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            m_storage.set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 16 位元有符號整數建構。
    /// </summary>
    /// <param name="v">16 位元整數</param>
    NUMERIC_CONSTEXPR_20 bigint(int16_t v) noexcept {
        if (v < 0) {
            m_storage.set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            m_storage.set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 32 位元有符號整數建構。
    /// </summary>
    /// <param name="v">32 位元整數</param>
    NUMERIC_CONSTEXPR_20 bigint(int32_t v) noexcept {
        if (v < 0) {
            m_storage.set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            m_storage.set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 64 位元有符號整數建構。
    /// </summary>
    /// <param name="v">64 位元整數</param>
    NUMERIC_CONSTEXPR_20 bigint(int64_t v) noexcept {
        if (v < 0) {
            m_storage.set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            m_storage.set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 8 位元無符號整數建構。
    /// </summary>
    /// <param name="v">8 位元無符號整數</param>
    NUMERIC_CONSTEXPR_20 bigint(uint8_t v) noexcept {
        m_storage.set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 16 位元無符號整數建構。
    /// </summary>
    /// <param name="v">16 位元無符號整數</param>
    NUMERIC_CONSTEXPR_20 bigint(uint16_t v) noexcept {
        m_storage.set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 32 位元無符號整數建構。
    /// </summary>
    /// <param name="v">32 位元無符號整數</param>
    NUMERIC_CONSTEXPR_20 bigint(uint32_t v) noexcept {
        m_storage.set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 64 位元無符號整數建構。
    /// </summary>
    /// <param name="v">64 位元無符號整數</param>
    NUMERIC_CONSTEXPR_20 bigint(uint64_t v) noexcept {
        m_storage.set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自其他原生整數型別（如 long, unsigned long, char）建構之泛型模板。
    /// </summary>
    /// <typeparam name="T">整數型別</typeparam>
    /// <param name="v">數值</param>
    template <typename T, typename std::enable_if<
        std::is_integral<T>::value &&
        !std::is_same<T, bool>::value &&
        !std::is_same<T, int8_t>::value &&
        !std::is_same<T, int16_t>::value &&
        !std::is_same<T, int32_t>::value &&
        !std::is_same<T, int64_t>::value &&
        !std::is_same<T, uint8_t>::value &&
        !std::is_same<T, uint16_t>::value &&
        !std::is_same<T, uint32_t>::value &&
        !std::is_same<T, uint64_t>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 bigint(T v) noexcept {
        if (std::is_signed<T>::value) {
            int64_t val = static_cast<int64_t>(v);
            if (val < 0) {
                m_storage.set_uint64(static_cast<uint64_t>(-(val + 1)) + 1, -1);
            } else {
                m_storage.set_uint64(static_cast<uint64_t>(val), val > 0 ? 1 : 0);
            }
        } else {
            uint64_t val = static_cast<uint64_t>(v);
            m_storage.set_uint64(val, val > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 string_view 解析並建構 bigint。
    /// </summary>
    /// <param name="sv">十進位字串視圖</param>
    /// <exception cref="std::invalid_argument">字串無效時拋出</exception>
    explicit NUMERIC_CONSTEXPR_20 bigint(numeric::string_view sv) {
        detail::BigIntCore::from_string(m_storage, sv);
    }

    /// <summary>
    /// 自 C-style 字串解析並建構 bigint。
    /// </summary>
    /// <param name="s">字串指標</param>
    /// <exception cref="std::invalid_argument">指標為 null 或格式不合法時拋出</exception>
    explicit NUMERIC_CONSTEXPR_20 bigint(const char* s) {
        if (s == nullptr) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("null string pointer"));
        }
        detail::BigIntCore::from_string(m_storage, numeric::string_view(s));
    }

    /// <summary>
    /// 自 std::string 解析並建構 bigint。
    /// </summary>
    /// <param name="s">字串物件</param>
    /// <exception cref="std::invalid_argument">字串格式不合法時拋出</exception>
    explicit bigint(const std::string& s) {
        detail::BigIntCore::from_string(m_storage, numeric::string_view(s.data(), s.size()));
    }

    /// <summary>
    /// 自 std::bitset 建構非負任意精度整數（N == 0 之特化處理）。
    /// </summary>
    /// <typeparam name="N">來源位元寬度</typeparam>
    template <size_t N, typename std::enable_if<(N == 0), int>::type = 0>
    bigint(const std::bitset<N>&) {}

    /// <summary>
    /// 自 std::bitset 建構非負任意精度整數（按無符號二進位數解析）。
    /// </summary>
    /// <typeparam name="N">來源位元寬度</typeparam>
    /// <param name="bs">來源 bitset 物件</param>
    template <size_t N, typename std::enable_if<(N > 0), int>::type = 0>
    bigint(const std::bitset<N>& bs) {
        size_t limb_cnt = (N + 63) / 64;
        m_storage.resize(limb_cnt, 0);
        bool any_bit = false;
        for (size_t w = 0; w < limb_cnt; ++w) {
            uint64_t val = 0;
            size_t bits_in_limb = (w == limb_cnt - 1) ? (N - w * 64) : 64;
            for (size_t b = 0; b < bits_in_limb; ++b) {
                if (bs.test(w * 64 + b)) {
                    val |= (static_cast<uint64_t>(1) << b);
                    any_bit = true;
                }
            }
            m_storage.data()[w] = val;
        }
        if (any_bit) {
            m_storage.m_sign = 1;
            m_storage.normalize();
        } else {
            m_storage.m_size = 0;
            m_storage.m_sign = 0;
            m_storage.shrink_to_sbo_if_possible();
        }
    }

    /// <summary>
    /// 靜態輔助方法：自字串解析 bigint。
    /// </summary>
    /// <param name="sv">十進位字串視圖</param>
    /// <returns>解析完成之 bigint 物件</returns>
    /// <exception cref="std::invalid_argument">字串為空或包含無效字元時拋出</exception>
    static NUMERIC_CONSTEXPR_20 bigint from_string(numeric::string_view sv) {
        bigint res;
        detail::BigIntCore::from_string(res.m_storage, sv);
        return res;
    }

    /// <summary>
    /// 靜態輔助方法：自二進位字串解析 bigint（支援正負符號與可選 "0b"/"0B" 前綴）。
    /// </summary>
    /// <param name="sv">二進位字串視圖</param>
    /// <returns>解析完成之 bigint 物件</returns>
    /// <exception cref="std::invalid_argument">字串為空或包含無效二進位字元時拋出</exception>
    static bigint from_binary_string(numeric::string_view sv) {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty binary string"));
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
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no binary digits in string"));
        }
        if (idx + 1 < sv.size() && sv[idx] == '0' && (sv[idx + 1] == 'b' || sv[idx + 1] == 'B')) {
            idx += 2;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no binary digits after prefix"));
        }
        // 驗證字元合法性
        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] != '0' && sv[i] != '1') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in binary string"));
            }
        }
        // 跳過前導 0
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            return bigint(0);
        }
        size_t num_bits = sv.size() - idx;
        size_t limb_cnt = (num_bits + 63) / 64;
        bigint res;
        res.m_storage.resize(limb_cnt, 0);
        for (size_t w = 0; w < limb_cnt; ++w) {
            uint64_t limb_val = 0;
            size_t bits_in_limb = (w == limb_cnt - 1) ? (num_bits - w * 64) : 64;
            for (size_t b = 0; b < bits_in_limb; ++b) {
                size_t char_pos = sv.size() - 1 - (w * 64 + b);
                if (sv[char_pos] == '1') {
                    limb_val |= (static_cast<uint64_t>(1) << b);
                }
            }
            res.m_storage.data()[w] = limb_val;
        }
        res.m_storage.m_sign = sign;
        res.m_storage.normalize();
        return res;
    }

    /// <summary>
    /// 查詢是否正在使用 SBO 內建緩衝區（無堆積配置，預設 256 位元 / 4 limbs）。
    /// </summary>
    /// <returns>若使用 SBO 回傳 true，否則回傳 false</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_sbo() const noexcept {
        return m_storage.is_sbo();
    }

    /// <summary>
    /// 查詢是否為小數值（SBO 模式之別名）。
    /// </summary>
    /// <returns>若為 SBO 儲存回傳 true</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_small() const noexcept {
        return m_storage.is_sbo();
    }

    /// <summary>
    /// 查詢數值是否為 0。
    /// </summary>
    /// <returns>若為零回傳 true</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_zero() const noexcept {
        return m_storage.m_size == 0 || (m_storage.m_size == 1 && m_storage.data()[0] == 0);
    }

    /// <summary>
    /// 取得數值符號。
    /// </summary>
    /// <returns>負數為 -1，零為 0，正數為 1</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 int8_t sign() const noexcept {
        return m_storage.m_sign;
    }

    /// <summary>
    /// 取得有效 64-bit limbs 數量。
    /// </summary>
    /// <returns>limbs 數量</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t limb_count() const noexcept {
        return m_storage.m_size;
    }

    /// <summary>
    /// 取得內部儲存層之 limbs 唯讀指標。
    /// </summary>
    /// <returns>唯讀 uint64_t 指標</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t* limbs() const noexcept {
        return m_storage.data();
    }

    /// <summary>
    /// 取得儲存層物件之內部非 const 參考。
    /// </summary>
    /// <returns>儲存層物件參考</returns>
    NUMERIC_CONSTEXPR_20 detail::BigIntStorage& storage() noexcept {
        return m_storage;
    }

    /// <summary>
    /// 取得儲存層物件之內部 const 參考。
    /// </summary>
    /// <returns>儲存層物件 const 參考</returns>
    NUMERIC_CONSTEXPR_20 const detail::BigIntStorage& storage() const noexcept {
        return m_storage;
    }

    /// <summary>
    /// 明確轉型為布林值（符合 C 語言非 0 為 true、0 為 false）。
    /// </summary>
    /// <returns>非零時回傳 true，零時回傳 false</returns>
    explicit NUMERIC_CONSTEXPR_20 operator bool() const noexcept {
        return m_storage.m_sign != 0;
    }

    /// <summary>
    /// 邏輯非運算子。
    /// </summary>
    /// <returns>若值為 0 回傳 true，否則回傳 false</returns>
    NUMERIC_CONSTEXPR_20 bool operator!() const noexcept {
        return m_storage.m_sign == 0;
    }

    /// <summary>
    /// 明確轉型為 64 位元有符號整數（可能截斷）。
    /// </summary>
    /// <returns>int64_t 數值</returns>
    explicit NUMERIC_CONSTEXPR_20 operator int64_t() const noexcept {
        if (m_storage.m_size == 0) return 0;
        uint64_t mag = m_storage.data()[0];
        if (m_storage.m_sign < 0) {
            return -static_cast<int64_t>(mag);
        }
        return static_cast<int64_t>(mag);
    }

    /// <summary>
    /// 明確轉型為 64 位元無符號整數（可能截斷）。
    /// </summary>
    /// <returns>uint64_t 數值</returns>
    explicit NUMERIC_CONSTEXPR_20 operator uint64_t() const noexcept {
        if (m_storage.m_size == 0) return 0;
        return m_storage.data()[0];
    }

    /// <summary>
    /// 明確轉型為 32 位元有符號整數（可能截斷）。
    /// </summary>
    /// <returns>int32_t 數值</returns>
    explicit NUMERIC_CONSTEXPR_20 operator int32_t() const noexcept {
        return static_cast<int32_t>(static_cast<int64_t>(*this));
    }

    /// <summary>
    /// 明確轉型為 32 位元無符號整數（可能截斷）。
    /// </summary>
    /// <returns>uint32_t 數值</returns>
    explicit NUMERIC_CONSTEXPR_20 operator uint32_t() const noexcept {
        return static_cast<uint32_t>(static_cast<uint64_t>(*this));
    }

    /// <summary>
    /// 明確轉型為 double 浮點數。
    /// </summary>
    /// <returns>近似 double 數值</returns>
    explicit operator double() const noexcept {
        if (m_storage.m_size == 0) return 0.0;
        double res = 0.0;
        double base = 1.0;
        const double two_pow_64 = 18446744073709551616.0;
        for (size_t i = 0; i < m_storage.m_size; ++i) {
            res += static_cast<double>(m_storage.data()[i]) * base;
            base *= two_pow_64;
        }
        return (m_storage.m_sign < 0) ? -res : res;
    }

    /// <summary>
    /// 轉換為十進位字串表示。
    /// </summary>
    /// <returns>十進位字串</returns>
    NUMERIC_NODISCARD std::string to_string() const {
        return detail::BigIntCore::to_string(m_storage);
    }

    /// <summary>
    /// 轉換為指定位元寬度之 std::bitset，負數時採用標準二補數表示法。
    /// </summary>
    /// <typeparam name="N">目標位元寬度</typeparam>
    /// <returns>對應之 std::bitset 物件</returns>
    template <size_t N>
    std::bitset<N> to_bitset() const {
        std::bitset<N> bs;
        if (N == 0) {
            return bs;
        }
        size_t limb_cnt = (N + 63) / 64;
        if (m_storage.m_sign >= 0) {
            for (size_t w = 0; w < limb_cnt; ++w) {
                uint64_t val = (w < m_storage.m_size) ? m_storage.data()[w] : 0ULL;
                size_t bits_in_limb = (w == limb_cnt - 1) ? (N - w * 64) : 64;
                for (size_t b = 0; b < bits_in_limb; ++b) {
                    if ((val >> b) & 1ULL) {
                        bs.set(w * 64 + b);
                    }
                }
            }
        } else {
            // 負數二補數計算：~magnitude + 1
            uint64_t carry = 1;
            for (size_t w = 0; w < limb_cnt; ++w) {
                uint64_t mag_w = (w < m_storage.m_size) ? m_storage.data()[w] : 0ULL;
                uint64_t inv_w = ~mag_w;
                uint64_t val = inv_w + carry;
                carry = (val < inv_w) ? 1 : 0;
                size_t bits_in_limb = (w == limb_cnt - 1) ? (N - w * 64) : 64;
                for (size_t b = 0; b < bits_in_limb; ++b) {
                    if ((val >> b) & 1ULL) {
                        bs.set(w * 64 + b);
                    }
                }
            }
        }
        return bs;
    }

    /// <summary>
    /// 轉換為二進位字串表示（負數具有 '-' 前綴，零為 "0"）。
    /// </summary>
    /// <returns>二進位字串</returns>
    NUMERIC_NODISCARD std::string to_binary_string() const {
        if (m_storage.m_sign == 0 || m_storage.m_size == 0) {
            return "0";
        }
        size_t high_limb_idx = m_storage.m_size - 1;
        uint64_t high_val = m_storage.data()[high_limb_idx];
        int leading_zeros = detail::BigIntCore::clz64(high_val);
        int bits_in_high = 64 - leading_zeros;
        size_t total_bits = high_limb_idx * 64 + static_cast<size_t>(bits_in_high);

        std::string result;
        result.reserve((m_storage.m_sign < 0 ? 1 : 0) + total_bits);
        if (m_storage.m_sign < 0) {
            result.push_back('-');
        }
        for (int b = bits_in_high - 1; b >= 0; --b) {
            result.push_back(((high_val >> b) & 1ULL) ? '1' : '0');
        }
        for (size_t i = high_limb_idx; i > 0; --i) {
            uint64_t val = m_storage.data()[i - 1];
            for (int b = 63; b >= 0; --b) {
                result.push_back(((val >> b) & 1ULL) ? '1' : '0');
            }
        }
        return result;
    }

    /// <summary>
    /// 一元正號運算子。
    /// </summary>
    /// <returns>自身之副本</returns>
    NUMERIC_CONSTEXPR_20 bigint operator+() const {
        return *this;
    }

    /// <summary>
    /// 一元負號運算子。
    /// </summary>
    /// <returns>正負號反轉後之結果</returns>
    NUMERIC_CONSTEXPR_20 bigint operator-() const {
        bigint res = *this;
        res.m_storage.m_sign = -res.m_storage.m_sign;
        return res;
    }

    /// <summary>
    /// 前置遞增運算子：++a。
    /// </summary>
    /// <returns>遞增後之自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator++() {
        *this += 1;
        return *this;
    }

    /// <summary>
    /// 後置遞增運算子：a++。
    /// </summary>
    /// <returns>遞增前之舊值</returns>
    NUMERIC_CONSTEXPR_20 bigint operator++(int) {
        bigint tmp = *this;
        *this += 1;
        return tmp;
    }

    /// <summary>
    /// 前置遞減運算子：--a。
    /// </summary>
    /// <returns>遞減後之自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator--() {
        *this -= 1;
        return *this;
    }

    /// <summary>
    /// 後置遞減運算子：a--。
    /// </summary>
    /// <returns>遞減前之舊值</returns>
    NUMERIC_CONSTEXPR_20 bigint operator--(int) {
        bigint tmp = *this;
        *this -= 1;
        return tmp;
    }

    /// <summary>
    /// 位元非運算子：~a = -a - 1。
    /// </summary>
    /// <returns>反轉後之數值</returns>
    NUMERIC_CONSTEXPR_20 bigint operator~() const {
        bigint res;
        detail::BigIntCore::bitwise_not(res.m_storage, m_storage);
        return res;
    }

    /// <summary>
    /// 加法複合賦值運算子。
    /// </summary>
    /// <param name="rhs">加數</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator+=(const bigint& rhs) {
        detail::BigIntCore::add_signed(m_storage, m_storage, rhs.m_storage);
        return *this;
    }

    /// <summary>
    /// 減法複合賦值運算子。
    /// </summary>
    /// <param name="rhs">減數</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator-=(const bigint& rhs) {
        detail::BigIntCore::sub_signed(m_storage, m_storage, rhs.m_storage);
        return *this;
    }

    /// <summary>
    /// 乘法複合賦值運算子。
    /// </summary>
    /// <param name="rhs">乘數</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator*=(const bigint& rhs) {
        detail::BigIntStorage tmp;
        detail::BigIntCore::mul_signed(tmp, m_storage, rhs.m_storage);
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 除法複合賦值運算子（內部僅求商數，0 額外餘數分配開銷）。
    /// </summary>
    /// <param name="rhs">除數</param>
    /// <returns>自身參考</returns>
    /// <exception cref="std::invalid_argument">除數為 0 時拋出</exception>
    NUMERIC_CONSTEXPR_20 bigint& operator/=(const bigint& rhs) {
        detail::BigIntStorage q;
        detail::BigIntCore::div_q_signed(q, m_storage, rhs.m_storage);
        m_storage = std::move(q);
        return *this;
    }

    /// <summary>
    /// 取模複合賦值運算子（內部僅求餘數，0 額外商數分配開銷）。
    /// </summary>
    /// <param name="rhs">除數</param>
    /// <returns>自身參考</returns>
    /// <exception cref="std::invalid_argument">除數為 0 時拋出</exception>
    NUMERIC_CONSTEXPR_20 bigint& operator%=(const bigint& rhs) {
        detail::BigIntStorage r;
        detail::BigIntCore::div_r_signed(r, m_storage, rhs.m_storage);
        m_storage = std::move(r);
        return *this;
    }

    /// <summary>
    /// 位元及複合賦值運算子。
    /// </summary>
    /// <param name="rhs">運算元</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator&=(const bigint& rhs) {
        detail::BigIntStorage tmp;
        detail::BigIntCore::bitwise_and(tmp, m_storage, rhs.m_storage);
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 位元或複合賦值運算子。
    /// </summary>
    /// <param name="rhs">運算元</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator|=(const bigint& rhs) {
        detail::BigIntStorage tmp;
        detail::BigIntCore::bitwise_or(tmp, m_storage, rhs.m_storage);
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 位元互斥或複合賦值運算子。
    /// </summary>
    /// <param name="rhs">運算元</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 bigint& operator^=(const bigint& rhs) {
        detail::BigIntStorage tmp;
        detail::BigIntCore::bitwise_xor(tmp, m_storage, rhs.m_storage);
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 位元左移複合賦值運算子。
    /// </summary>
    /// <typeparam name="T">整數型別</typeparam>
    /// <param name="shift">位移位元數</param>
    /// <returns>自身參考</returns>
    /// <exception cref="std::invalid_argument">位移為負數時拋出</exception>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 bigint& operator<<=(T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        detail::BigIntStorage tmp;
        detail::BigIntCore::shift_left(tmp, m_storage, static_cast<size_t>(shift));
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 位元右移複合賦值運算子。
    /// </summary>
    /// <typeparam name="T">整數型別</typeparam>
    /// <param name="shift">位移位元數</param>
    /// <returns>自身參考</returns>
    /// <exception cref="std::invalid_argument">位移為負數時拋出</exception>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 bigint& operator>>=(T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        detail::BigIntStorage tmp;
        detail::BigIntCore::shift_right(tmp, m_storage, static_cast<size_t>(shift));
        m_storage = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 雙目加法運算子（常數參考傳遞，消除堆積深層複製開銷）。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>加法結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator+(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::add_signed(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator+(bigint&& lhs, const bigint& rhs) {
        lhs += rhs;
        return std::move(lhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator+(const bigint& lhs, bigint&& rhs) {
        rhs += lhs;
        return std::move(rhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator+(bigint&& lhs, bigint&& rhs) {
        lhs += rhs;
        return std::move(lhs);
    }

    /// <summary>
    /// 雙目減法運算子（常數參考傳遞，消除堆積深層複製開銷）。
    /// </summary>
    /// <param name="lhs">被減數</param>
    /// <param name="rhs">減數</param>
    /// <returns>減法結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator-(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::sub_signed(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator-(bigint&& lhs, const bigint& rhs) {
        lhs -= rhs;
        return std::move(lhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator-(bigint&& lhs, bigint&& rhs) {
        lhs -= rhs;
        return std::move(lhs);
    }

    /// <summary>
    /// 雙目乘法運算子（左運算元採常數參考傳遞，消除堆積深層複製開銷）。
    /// </summary>
    /// <param name="lhs">乘數</param>
    /// <param name="rhs">乘數</param>
    /// <returns>乘法結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator*(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::mul_signed(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    /// <summary>
    /// 雙目除法運算子（左運算元採常數參考傳遞，內部僅求商數，0 額外餘數分配開銷）。
    /// </summary>
    /// <param name="lhs">被除數</param>
    /// <param name="rhs">除數</param>
    /// <returns>商</returns>
    /// <exception cref="std::invalid_argument">除數為 0 時拋出</exception>
    friend NUMERIC_CONSTEXPR_20 bigint operator/(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::div_q_signed(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    /// <summary>
    /// 雙目取模運算子（左運算元採常數參考傳遞，內部僅求餘數，0 額外商數分配開銷）。
    /// </summary>
    /// <param name="lhs">被除數</param>
    /// <param name="rhs">除數</param>
    /// <returns>餘數</returns>
    /// <exception cref="std::invalid_argument">除數為 0 時拋出</exception>
    friend NUMERIC_CONSTEXPR_20 bigint operator%(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::div_r_signed(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    /// <summary>
    /// 雙目位元及運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>運算結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator&(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::bitwise_and(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator&(bigint&& lhs, const bigint& rhs) {
        lhs &= rhs;
        return std::move(lhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator&(const bigint& lhs, bigint&& rhs) {
        rhs &= lhs;
        return std::move(rhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator&(bigint&& lhs, bigint&& rhs) {
        lhs &= rhs;
        return std::move(lhs);
    }

    /// <summary>
    /// 雙目位元或運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>運算結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator|(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::bitwise_or(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator|(bigint&& lhs, const bigint& rhs) {
        lhs |= rhs;
        return std::move(lhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator|(const bigint& lhs, bigint&& rhs) {
        rhs |= lhs;
        return std::move(rhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator|(bigint&& lhs, bigint&& rhs) {
        lhs |= rhs;
        return std::move(lhs);
    }

    /// <summary>
    /// 雙目位元互斥或運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>運算結果</returns>
    friend NUMERIC_CONSTEXPR_20 bigint operator^(const bigint& lhs, const bigint& rhs) {
        bigint result;
        detail::BigIntCore::bitwise_xor(result.m_storage, lhs.m_storage, rhs.m_storage);
        return result;
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator^(bigint&& lhs, const bigint& rhs) {
        lhs ^= rhs;
        return std::move(lhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator^(const bigint& lhs, bigint&& rhs) {
        rhs ^= lhs;
        return std::move(rhs);
    }

    friend NUMERIC_CONSTEXPR_20 bigint operator^(bigint&& lhs, bigint&& rhs) {
        lhs ^= rhs;
        return std::move(lhs);
    }

    /// <summary>
    /// 位元左移運算子。
    /// </summary>
    /// <typeparam name="T">整數型別</typeparam>
    /// <param name="lhs">運算元</param>
    /// <param name="shift">位移位元數</param>
    /// <returns>位移結果</returns>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bigint operator<<(const bigint& lhs, T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        bigint result;
        detail::BigIntCore::shift_left(result.m_storage, lhs.m_storage, static_cast<size_t>(shift));
        return result;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bigint operator<<(bigint&& lhs, T shift) {
        lhs <<= shift;
        return std::move(lhs);
    }

    /// <summary>
    /// 位元右移運算子。
    /// </summary>
    /// <typeparam name="T">整數型別</typeparam>
    /// <param name="lhs">運算元</param>
    /// <param name="shift">位移位元數</param>
    /// <returns>位移結果</returns>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bigint operator>>(const bigint& lhs, T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        bigint result;
        detail::BigIntCore::shift_right(result.m_storage, lhs.m_storage, static_cast<size_t>(shift));
        return result;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bigint operator>>(bigint&& lhs, T shift) {
        lhs >>= shift;
        return std::move(lhs);
    }

    /// <summary>
    /// 相等比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若數值相等回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator==(const bigint& lhs, const bigint& rhs) noexcept {
        if (lhs.m_storage.m_sign != rhs.m_storage.m_sign) return false;
        if (lhs.m_storage.m_size != rhs.m_storage.m_size) return false;
        if (lhs.m_storage.m_size == 0) return true;
        for (size_t i = 0; i < lhs.m_storage.m_size; ++i) {
            if (lhs.m_storage.data()[i] != rhs.m_storage.data()[i]) return false;
        }
        return true;
    }

    /// <summary>
    /// 不相等比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若數值不相等回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator!=(const bigint& lhs, const bigint& rhs) noexcept {
        return !(lhs == rhs);
    }

    /// <summary>
    /// 小於比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若 lhs &lt; rhs 回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator<(const bigint& lhs, const bigint& rhs) noexcept {
        if (lhs.m_storage.m_sign < rhs.m_storage.m_sign) return true;
        if (lhs.m_storage.m_sign > rhs.m_storage.m_sign) return false;
        if (lhs.m_storage.m_sign == 0) return false;
        int cmp = detail::BigIntCore::compare_unsigned(
            lhs.m_storage.data(), lhs.m_storage.m_size,
            rhs.m_storage.data(), rhs.m_storage.m_size);
        if (lhs.m_storage.m_sign > 0) {
            return cmp < 0;
        } else {
            return cmp > 0;
        }
    }

    /// <summary>
    /// 小於等於比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若 lhs &lt;= rhs 回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator<=(const bigint& lhs, const bigint& rhs) noexcept {
        return !(rhs < lhs);
    }

    /// <summary>
    /// 大於比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若 lhs &gt; rhs 回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator>(const bigint& lhs, const bigint& rhs) noexcept {
        return rhs < lhs;
    }

    /// <summary>
    /// 大於等於比較運算子。
    /// </summary>
    /// <param name="lhs">左運算元</param>
    /// <param name="rhs">右運算元</param>
    /// <returns>若 lhs &gt;= rhs 回傳 true</returns>
    friend NUMERIC_CONSTEXPR_20 bool operator>=(const bigint& lhs, const bigint& rhs) noexcept {
        return !(lhs < rhs);
    }

    /// <summary>
    /// 輸出串流運算子。
    /// </summary>
    /// <param name="os">目標輸出串流</param>
    /// <param name="val">待輸出之 bigint</param>
    /// <returns>串流參考</returns>
    friend std::ostream& operator<<(std::ostream& os, const bigint& val) {
        os << val.to_string();
        return os;
    }
};

namespace detail {

    NUMERIC_CONSTEXPR_20 BigIntConstantProxy::operator bigint() const {
        return bigint(value);
    }

    NUMERIC_CONSTEXPR_20 bigint BigIntConstantProxy::operator()() const {
        return bigint(value);
    }

    template <typename T>
    NUMERIC_CONSTEXPR_20 bool operator==(BigIntConstantProxy p, const T& other) {
        return bigint(p.value) == other;
    }

    template <typename T>
    NUMERIC_CONSTEXPR_20 bool operator==(const T& other, BigIntConstantProxy p) {
        return other == bigint(p.value);
    }

    template <typename T>
    NUMERIC_CONSTEXPR_20 bool operator!=(BigIntConstantProxy p, const T& other) {
        return bigint(p.value) != other;
    }

    template <typename T>
    NUMERIC_CONSTEXPR_20 bool operator!=(const T& other, BigIntConstantProxy p) {
        return other != bigint(p.value);
    }

} // namespace detail

/// <summary>
/// 雙目邏輯及運算子。
/// </summary>
/// <param name="lhs">左運算元</param>
/// <param name="rhs">右運算元</param>
/// <returns>邏輯及結果</returns>
NUMERIC_CONSTEXPR_20 bool operator&&(const bigint& lhs, const bigint& rhs) noexcept {
    return static_cast<bool>(lhs) && static_cast<bool>(rhs);
}

/// <summary>
/// 雙目邏輯或運算子。
/// </summary>
/// <param name="lhs">左運算元</param>
/// <param name="rhs">右運算元</param>
/// <returns>邏輯或結果</returns>
NUMERIC_CONSTEXPR_20 bool operator||(const bigint& lhs, const bigint& rhs) noexcept {
    return static_cast<bool>(lhs) || static_cast<bool>(rhs);
}

/// <summary>
/// bigint 與原生整數/布林型別之混合邏輯及運算子（左 bigint 右原生）。
/// </summary>
/// <typeparam name="T">整數或布林型別</typeparam>
/// <param name="lhs">左 bigint</param>
/// <param name="rhs">右原生數值</param>
/// <returns>邏輯及結果</returns>
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator&&(const bigint& lhs, T rhs) noexcept {
    return static_cast<bool>(lhs) && (rhs != 0);
}

/// <summary>
/// bigint 與原生整數/布林型別之混合邏輯及運算子（左原生右 bigint）。
/// </summary>
/// <typeparam name="T">整數或布林型別</typeparam>
/// <param name="lhs">左原生數值</param>
/// <param name="rhs">右 bigint</param>
/// <returns>邏輯及結果</returns>
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator&&(T lhs, const bigint& rhs) noexcept {
    return (lhs != 0) && static_cast<bool>(rhs);
}

/// <summary>
/// bigint 與原生整數/布林型別之混合邏輯或運算子（左 bigint 右原生）。
/// </summary>
/// <typeparam name="T">整數或布林型別</typeparam>
/// <param name="lhs">左 bigint</param>
/// <param name="rhs">右原生數值</param>
/// <returns>邏輯或結果</returns>
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator||(const bigint& lhs, T rhs) noexcept {
    return static_cast<bool>(lhs) || (rhs != 0);
}

/// <summary>
/// bigint 與原生整數/布林型別之混合邏輯或運算子（左原生右 bigint）。
/// </summary>
/// <typeparam name="T">整數或布林型別</typeparam>
/// <param name="lhs">左原生數值</param>
/// <param name="rhs">右 bigint</param>
/// <returns>邏輯或結果</returns>
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator||(T lhs, const bigint& rhs) noexcept {
    return (lhs != 0) || static_cast<bool>(rhs);
}

} // namespace numeric

#if NUMERIC_HAS_STD_FORMAT
#include <format>

namespace std {

/// <summary>
/// std::formatter specialization for numeric::bigint.
/// </summary>
/// <typeparam name="CharT">Character type.</typeparam>
template <typename CharT>
struct formatter<numeric::bigint, CharT> {
    /// <summary>
    /// Parses format specifications for bigint.
    /// </summary>
    /// <typeparam name="ParseContext">Format parse context type.</typeparam>
    /// <param name="ctx">Parse context reference.</param>
    /// <returns>Iterator pointing to the end of format specification.</returns>
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        auto end = ctx.end();
        if (it != end && *it == ':') {
            ++it;
        }
        if (it != end && *it != '}') {
            throw std::format_error("invalid format specifier for bigint");
        }
        return it;
    }

    /// <summary>
    /// Formats numeric::bigint into decimal string representation.
    /// </summary>
    /// <typeparam name="FormatContext">Format context type.</typeparam>
    /// <param name="val">The bigint value to format.</param>
    /// <param name="ctx">Format context reference.</param>
    /// <returns>Updated output iterator.</returns>
    template <typename FormatContext>
    auto format(const numeric::bigint& val, FormatContext& ctx) const -> decltype(ctx.out()) {
        std::string s = val.to_string();
        auto it = ctx.out();
        for (char c : s) {
            *it++ = static_cast<CharT>(c);
        }
        return it;
    }
};

} // namespace std
#endif // NUMERIC_HAS_STD_FORMAT

namespace std {

/// <summary>
/// std::hash specialization for numeric::bigint.
/// </summary>
template <>
struct hash<numeric::bigint> {
    /// <summary>
    /// Computes hash value for numeric::bigint using FNV-1a with MurmurHash avalanche finalizer.
    /// </summary>
    /// <param name="val">The bigint value to hash.</param>
    /// <returns>Hash value.</returns>
    size_t operator()(const numeric::bigint& val) const noexcept {
        if (val.sign() == 0) {
            return 0;
        }
        uint64_t h = 14695981039346656037ULL;
        const uint64_t fnv_prime = 1099511628211ULL;

        uint64_t s = static_cast<uint64_t>(val.sign() < 0 ? 1 : 2);
        h ^= s;
        h *= fnv_prime;

        const uint64_t* limbs = val.limbs();
        size_t n = val.limb_count();
        for (size_t i = 0; i < n; ++i) {
            h ^= limbs[i];
            h *= fnv_prime;
        }

        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        return static_cast<size_t>(h);
#else
        return static_cast<size_t>(h ^ (h >> 32));
#endif
    }
};

} // namespace std

#ifndef NUMERIC_NO_GLOBAL_TYPE_ALIAS
using numeric::bigint;
#endif


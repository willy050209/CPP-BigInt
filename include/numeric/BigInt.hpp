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

template <size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
class BasicBigInt;

using BigInt     = BasicBigInt<NUMERIC_BIGINT_SBO_LIMBS>;
using bigint     = BigInt;
using BigInt256  = BasicBigInt<4>;
using bigint256  = BigInt256;
using BigInt512  = BasicBigInt<8>;
using bigint512  = BigInt512;
using BigInt1024 = BasicBigInt<16>;
using bigint1024 = BigInt1024;

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
        /// 純量轉換至 int64_t。
        /// </summary>
        constexpr explicit operator int64_t() const noexcept { return value; }

        /// <summary>
        /// 條件判斷布林轉換。
        /// </summary>
        constexpr explicit operator bool() const noexcept { return value != 0; }

        /// <summary>
        /// 隱式轉換至 BasicBigInt<SboLimbs>。
        /// </summary>
        template <size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
        NUMERIC_CONSTEXPR_20 operator BasicBigInt<SboLimbs>() const noexcept;

        /// <summary>
        /// 函式呼叫運算子，傳回對應之 BasicBigInt<SboLimbs>。
        /// </summary>
        template <size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
        NUMERIC_CONSTEXPR_20 BasicBigInt<SboLimbs> operator()() const noexcept;

        friend constexpr bool operator==(BigIntConstantProxy a, BigIntConstantProxy b) noexcept {
            return a.value == b.value;
        }

        friend constexpr bool operator!=(BigIntConstantProxy a, BigIntConstantProxy b) noexcept {
            return a.value != b.value;
        }

        template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        friend NUMERIC_CONSTEXPR_20 bool operator==(BigIntConstantProxy p, T other) noexcept {
            return p.value == static_cast<int64_t>(other);
        }

        template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        friend NUMERIC_CONSTEXPR_20 bool operator==(T other, BigIntConstantProxy p) noexcept {
            return static_cast<int64_t>(other) == p.value;
        }

        template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(BigIntConstantProxy p, T other) noexcept {
            return p.value != static_cast<int64_t>(other);
        }

        template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(T other, BigIntConstantProxy p) noexcept {
            return static_cast<int64_t>(other) != p.value;
        }

        template <size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 bool operator==(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept;

        template <size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 bool operator==(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept;

        template <size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept;

        template <size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 bool operator!=(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept;
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
/// 任意精度整數類別，採用 LLVM SmallVector 雙層架構，SBO 容量支援編譯期樣板參數化。
/// 衍生自非樣板核心 BigIntBase，重量級演算法不隨 SBO 展開，杜絕 Code Bloat。
/// </summary>
/// <typeparam name="SboLimbs">靜態 SBO 內聯肢數（預設為 NUMERIC_BIGINT_SBO_LIMBS = 4）。</typeparam>
template <size_t SboLimbs>
class BasicBigInt : public BigIntBase, public detail::BigIntConstants<> {
private:
    static constexpr size_t ACTUAL_SBO = (SboLimbs > 0) ? SboLimbs : 1;
    alignas(uint64_t) uint64_t m_inline_storage[ACTUAL_SBO];

public:
    static constexpr detail::BigIntConstantProxy zero{0};
    static constexpr detail::BigIntConstantProxy one{1};

    /// <summary>
    /// 預設建構子：初始化數值為 0。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt() noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
    }

    /// <summary>
    /// 複製建構子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(const BasicBigInt& other)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->assign_from(other);
    }

    /// <summary>
    /// 跨 SBO 容量複製建構子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt(const BasicBigInt<OtherLimbs>& other)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->assign_from(other);
    }

    /// <summary>
    /// 移動建構子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(BasicBigInt&& other) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->move_from(std::move(other));
    }

    /// <summary>
    /// 跨 SBO 容量移動建構子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt(BasicBigInt<OtherLimbs>&& other) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->move_from(std::move(other));
    }

    /// <summary>
    /// 複製賦值運算子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator=(const BasicBigInt& other) {
        this->assign_from(other);
        return *this;
    }

    /// <summary>
    /// 跨 SBO 容量複製賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator=(const BasicBigInt<OtherLimbs>& other) {
        this->assign_from(other);
        return *this;
    }

    /// <summary>
    /// 移動賦值運算子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator=(BasicBigInt&& other) noexcept {
        this->move_from(std::move(other));
        return *this;
    }

    /// <summary>
    /// 跨 SBO 容量移動賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator=(BasicBigInt<OtherLimbs>&& other) noexcept {
        this->move_from(std::move(other));
        return *this;
    }

    /// <summary>
    /// 自常數代理物件指派賦值。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator=(detail::BigIntConstantProxy proxy) noexcept {
        return *this = BasicBigInt(proxy.value);
    }

    /// <summary>
    /// 解構子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~BasicBigInt() = default;

    /// <summary>
    /// 自常數代理物件（如 bigint::zero, bigint::one）直接建構，避免透過轉換運算子樣板推導。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(detail::BigIntConstantProxy proxy) noexcept
        : BasicBigInt(proxy.value) {}

    /// <summary>
    /// 自內部 BigIntStorage 建立 BasicBigInt。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(detail::BigIntStorage storage) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->move_from(std::move(storage));
    }

    /// <summary>
    /// 自布林值建構：true 為 1，false 為 0。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(bool b) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->set_uint64(b ? 1 : 0, b ? 1 : 0);
    }

    /// <summary>
    /// 自 8 位元有符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(int8_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        if (v < 0) {
            this->set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            this->set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 16 位元有符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(int16_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        if (v < 0) {
            this->set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            this->set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 32 位元有符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(int32_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        if (v < 0) {
            this->set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            this->set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 64 位元有符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(int64_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        if (v < 0) {
            this->set_uint64(static_cast<uint64_t>(-(v + 1)) + 1, -1);
        } else {
            this->set_uint64(static_cast<uint64_t>(v), v > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 8 位元無符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(uint8_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 16 位元無符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(uint16_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 32 位元無符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(uint32_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自 64 位元無符號整數建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(uint64_t v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        this->set_uint64(v, v > 0 ? 1 : 0);
    }

    /// <summary>
    /// 自其他原生整數型別（如 long, unsigned long, char）建構之泛型模板。
    /// </summary>
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
    NUMERIC_CONSTEXPR_20 BasicBigInt(T v) noexcept
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        if (std::is_signed<T>::value) {
            int64_t val = static_cast<int64_t>(v);
            if (val < 0) {
                this->set_uint64(static_cast<uint64_t>(-(val + 1)) + 1, -1);
            } else {
                this->set_uint64(static_cast<uint64_t>(val), val > 0 ? 1 : 0);
            }
        } else {
            uint64_t val = static_cast<uint64_t>(v);
            this->set_uint64(val, val > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// 自 string_view 解析並建構 BasicBigInt。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(numeric::string_view sv)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        detail::BigIntCore::from_string(*this, sv);
    }

    /// <summary>
    /// 自 C-style 字串解析並建構 BasicBigInt。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(const char* s)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        if (s == nullptr) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("null string pointer"));
        }
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        detail::BigIntCore::from_string(*this, numeric::string_view(s));
    }

    /// <summary>
    /// 自 std::string 解析並建構 BasicBigInt。
    /// </summary>
    explicit BasicBigInt(const std::string& s)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        detail::BigIntCore::from_string(*this, numeric::string_view(s.data(), s.size()));
    }

    /// <summary>
    /// 自 std::bitset 建構非負任意精度整數（N == 0 之特化處理）。
    /// </summary>
    template <size_t N, typename std::enable_if<(N == 0), int>::type = 0>
    BasicBigInt(const std::bitset<N>&)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
    }

    /// <summary>
    /// 自 std::bitset 建構非負任意精度整數（按無符號二進位數解析）。
    /// </summary>
    template <size_t N, typename std::enable_if<(N > 0), int>::type = 0>
    BasicBigInt(const std::bitset<N>& bs)
        : BigIntBase(m_inline_storage, static_cast<uint32_t>(SboLimbs)) {
        zero_limbs(m_inline_storage, ACTUAL_SBO);
        size_t limb_cnt = (N + 63) / 64;
        this->resize(limb_cnt, 0);
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
            this->m_data[w] = val;
        }
        if (any_bit) {
            this->m_sign = 1;
            this->normalize();
        } else {
            this->m_size = 0;
            this->m_sign = 0;
            this->shrink_to_sbo_if_possible();
        }
    }

    /// <summary>
    /// 靜態輔助方法：自字串解析 BasicBigInt。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 BasicBigInt from_string(numeric::string_view sv) {
        BasicBigInt res;
        detail::BigIntCore::from_string(res, sv);
        return res;
    }

    /// <summary>
    /// 靜態輔助方法：自二進位字串解析 BasicBigInt。
    /// </summary>
    static BasicBigInt from_binary_string(numeric::string_view sv) {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty binary string"));
        }
        size_t idx = 0;
        int8_t sign_val = 1;
        if (sv[0] == '-') {
            sign_val = -1;
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
        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] != '0' && sv[i] != '1') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in binary string"));
            }
        }
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            return BasicBigInt(0);
        }
        size_t num_bits = sv.size() - idx;
        size_t limb_cnt = (num_bits + 63) / 64;
        BasicBigInt res;
        res.resize(limb_cnt, 0);
        for (size_t w = 0; w < limb_cnt; ++w) {
            uint64_t limb_val = 0;
            size_t bits_in_limb = (w == limb_cnt - 1) ? (num_bits - w * 64) : 64;
            for (size_t b = 0; b < bits_in_limb; ++b) {
                size_t char_pos = sv.size() - 1 - (w * 64 + b);
                if (sv[char_pos] == '1') {
                    limb_val |= (static_cast<uint64_t>(1) << b);
                }
            }
            res.m_data[w] = limb_val;
        }
        res.m_sign = sign_val;
        res.normalize();
        return res;
    }

    /// <summary>
    /// 取得內部儲存層之參考。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase& storage() noexcept {
        return *this;
    }

    /// <summary>
    /// 取得內部儲存層之常數參考。
    /// </summary>
    NUMERIC_CONSTEXPR_20 const BigIntBase& storage() const noexcept {
        return *this;
    }

    /// <summary>
    /// 明確轉型為布林值（非 0 為 true、0 為 false）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 operator bool() const noexcept {
        return this->m_sign != 0;
    }

    /// <summary>
    /// 邏輯非運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 bool operator!() const noexcept {
        return this->m_sign == 0;
    }

    /// <summary>
    /// 明確轉型為 64 位元有符號整數（可能截斷）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 operator int64_t() const noexcept {
        if (this->m_size == 0) return 0;
        uint64_t mag = this->m_data[0];
        if (this->m_sign < 0) {
            return -static_cast<int64_t>(mag);
        }
        return static_cast<int64_t>(mag);
    }

    /// <summary>
    /// 明確轉型為 64 位元無符號整數（可能截斷）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 operator uint64_t() const noexcept {
        if (this->m_size == 0) return 0;
        return this->m_data[0];
    }

    /// <summary>
    /// 明確轉型為 32 位元有符號整數（可能截斷）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 operator int32_t() const noexcept {
        return static_cast<int32_t>(static_cast<int64_t>(*this));
    }

    /// <summary>
    /// 明確轉型為 32 位元無符號整數（可能截斷）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 operator uint32_t() const noexcept {
        return static_cast<uint32_t>(static_cast<uint64_t>(*this));
    }

    /// <summary>
    /// 明確轉型為 double 浮點數。
    /// </summary>
    explicit operator double() const noexcept {
        if (this->m_size == 0) return 0.0;
        double res = 0.0;
        double base = 1.0;
        const double two_pow_64 = 18446744073709551616.0;
        for (size_t i = 0; i < this->m_size; ++i) {
            res += static_cast<double>(this->m_data[i]) * base;
            base *= two_pow_64;
        }
        return (this->m_sign < 0) ? -res : res;
    }

    /// <summary>
    /// 轉換為十進位字串表示。
    /// </summary>
    NUMERIC_NODISCARD std::string to_string() const {
        return detail::BigIntCore::to_string(*this);
    }

    /// <summary>
    /// 轉換為指定位元寬度之 std::bitset，負數時採用標準二補數表示法。
    /// </summary>
    template <size_t N>
    std::bitset<N> to_bitset() const {
        std::bitset<N> bs;
        if (N == 0) {
            return bs;
        }
        size_t limb_cnt = (N + 63) / 64;
        if (this->m_sign >= 0) {
            for (size_t w = 0; w < limb_cnt; ++w) {
                uint64_t val = (w < this->m_size) ? this->m_data[w] : 0ULL;
                size_t bits_in_limb = (w == limb_cnt - 1) ? (N - w * 64) : 64;
                for (size_t b = 0; b < bits_in_limb; ++b) {
                    if ((val >> b) & 1ULL) {
                        bs.set(w * 64 + b);
                    }
                }
            }
        } else {
            uint64_t carry = 1;
            for (size_t w = 0; w < limb_cnt; ++w) {
                uint64_t mag_w = (w < this->m_size) ? this->m_data[w] : 0ULL;
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
    /// 轉換為二進位字串表示。
    /// </summary>
    NUMERIC_NODISCARD std::string to_binary_string() const {
        if (this->m_sign == 0 || this->m_size == 0) {
            return "0";
        }
        size_t high_limb_idx = this->m_size - 1;
        uint64_t high_val = this->m_data[high_limb_idx];
        int leading_zeros = detail::BigIntCore::clz64(high_val);
        int bits_in_high = 64 - leading_zeros;
        size_t total_bits = high_limb_idx * 64 + static_cast<size_t>(bits_in_high);

        std::string result;
        result.reserve((this->m_sign < 0 ? 1 : 0) + total_bits);
        if (this->m_sign < 0) {
            result.push_back('-');
        }
        for (int b = bits_in_high - 1; b >= 0; --b) {
            result.push_back(((high_val >> b) & 1ULL) ? '1' : '0');
        }
        for (size_t i = high_limb_idx; i > 0; --i) {
            uint64_t val = this->m_data[i - 1];
            for (int b = 63; b >= 0; --b) {
                result.push_back(((val >> b) & 1ULL) ? '1' : '0');
            }
        }
        return result;
    }

    /// <summary>
    /// 一元正號運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator+() const {
        return *this;
    }

    /// <summary>
    /// 一元負號運算子（右值優化）：原地反轉正負號，保證零動態記憶體配置。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator-() && noexcept {
        this->negate();
        return std::move(*this);
    }

    /// <summary>
    /// 一元負號運算子（左值）：拷貝後反轉正負號。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator-() const & {
        BasicBigInt res(*this);
        res.negate();
        return res;
    }

    /// <summary>
    /// 前置遞增運算子：++a。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator++() {
        *this += 1;
        return *this;
    }

    /// <summary>
    /// 後置遞增運算子：a++。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator++(int) {
        BasicBigInt tmp = *this;
        *this += 1;
        return tmp;
    }

    /// <summary>
    /// 前置遞減運算子：--a。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator--() {
        *this -= 1;
        return *this;
    }

    /// <summary>
    /// 後置遞減運算子：a--。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator--(int) {
        BasicBigInt tmp = *this;
        *this -= 1;
        return tmp;
    }

    /// <summary>
    /// 位元非運算子：~a = -a - 1。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt operator~() const {
        BasicBigInt res;
        detail::BigIntCore::bitwise_not(res, *this);
        return res;
    }

    /// <summary>
    /// 加法複合賦值運算子（同容量或跨容量）。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator+=(const BasicBigInt<OtherLimbs>& rhs) {
        detail::BigIntCore::add_signed(*this, *this, rhs);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator+=(T rhs) {
        BasicBigInt tmp(rhs);
        detail::BigIntCore::add_signed(*this, *this, tmp);
        return *this;
    }

    /// <summary>
    /// 減法複合賦值運算子（同容量或跨容量）。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator-=(const BasicBigInt<OtherLimbs>& rhs) {
        detail::BigIntCore::sub_signed(*this, *this, rhs);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator-=(T rhs) {
        BasicBigInt tmp(rhs);
        detail::BigIntCore::sub_signed(*this, *this, tmp);
        return *this;
    }

    /// <summary>
    /// 乘法複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator*=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt tmp;
        detail::BigIntCore::mul_signed(tmp, *this, rhs);
        *this = std::move(tmp);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator*=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this *= tmp;
    }

    /// <summary>
    /// 除法複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator/=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt q;
        detail::BigIntCore::div_q_signed(q, *this, rhs);
        *this = std::move(q);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator/=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this /= tmp;
    }

    /// <summary>
    /// 取模複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator%=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt r;
        detail::BigIntCore::div_r_signed(r, *this, rhs);
        *this = std::move(r);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator%=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this %= tmp;
    }

    /// <summary>
    /// 位元及複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator&=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt tmp;
        detail::BigIntCore::bitwise_and(tmp, *this, rhs);
        *this = std::move(tmp);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator&=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this &= tmp;
    }

    /// <summary>
    /// 位元或複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator|=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt tmp;
        detail::BigIntCore::bitwise_or(tmp, *this, rhs);
        *this = std::move(tmp);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator|=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this |= tmp;
    }

    /// <summary>
    /// 位元互斥或複合賦值運算子。
    /// </summary>
    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator^=(const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt tmp;
        detail::BigIntCore::bitwise_xor(tmp, *this, rhs);
        *this = std::move(tmp);
        return *this;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator^=(T rhs) {
        BasicBigInt tmp(rhs);
        return *this ^= tmp;
    }

    /// <summary>
    /// 位元左移複合賦值運算子。
    /// </summary>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator<<=(T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        BasicBigInt tmp;
        detail::BigIntCore::shift_left(tmp, *this, static_cast<size_t>(shift));
        *this = std::move(tmp);
        return *this;
    }

    /// <summary>
    /// 位元右移複合賦值運算子。
    /// </summary>
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    NUMERIC_CONSTEXPR_20 BasicBigInt& operator>>=(T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        BasicBigInt tmp;
        detail::BigIntCore::shift_right(tmp, *this, static_cast<size_t>(shift));
        *this = std::move(tmp);
        return *this;
    }

    // --- 異質雙目運算子（以 LHS 容量為回傳基準） ---

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator+(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res(lhs);
        res += rhs;
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator+(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        lhs += rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator+(const BasicBigInt& lhs, T rhs) {
        BasicBigInt res(lhs);
        res += rhs;
        return res;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator+(BasicBigInt&& lhs, T rhs) {
        lhs += rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator+(T lhs, const BasicBigInt& rhs) {
        BasicBigInt res(lhs);
        res += rhs;
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator-(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res(lhs);
        res -= rhs;
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator-(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        lhs -= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator-(const BasicBigInt& lhs, T rhs) {
        BasicBigInt res(lhs);
        res -= rhs;
        return res;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator-(BasicBigInt&& lhs, T rhs) {
        lhs -= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator-(T lhs, const BasicBigInt& rhs) {
        BasicBigInt res(lhs);
        res -= rhs;
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator*(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::mul_signed(res, lhs, rhs);
        return res;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator*(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs * tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator*(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp * rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator/(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::div_q_signed(res, lhs, rhs);
        return res;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator/(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs / tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator/(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp / rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator%(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::div_r_signed(res, lhs, rhs);
        return res;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator%(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs % tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator%(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp % rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator&(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::bitwise_and(res, lhs, rhs);
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator&(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        lhs &= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator&(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs & tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator&(BasicBigInt&& lhs, T rhs) {
        lhs &= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator&(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp & rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator|(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::bitwise_or(res, lhs, rhs);
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator|(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        lhs |= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator|(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs | tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator|(BasicBigInt&& lhs, T rhs) {
        lhs |= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator|(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp | rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator^(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        BasicBigInt res;
        detail::BigIntCore::bitwise_xor(res, lhs, rhs);
        return res;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator^(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) {
        lhs ^= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator^(const BasicBigInt& lhs, T rhs) {
        BasicBigInt tmp(rhs);
        return lhs ^ tmp;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator^(BasicBigInt&& lhs, T rhs) {
        lhs ^= rhs;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator^(T lhs, const BasicBigInt& rhs) {
        BasicBigInt tmp(lhs);
        return tmp ^ rhs;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator<<(const BasicBigInt& lhs, T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        BasicBigInt result;
        detail::BigIntCore::shift_left(result, lhs, static_cast<size_t>(shift));
        return result;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator<<(BasicBigInt&& lhs, T shift) {
        lhs <<= shift;
        return std::move(lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator>>(const BasicBigInt& lhs, T shift) {
        if (shift < 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
        }
        BasicBigInt result;
        detail::BigIntCore::shift_right(result, lhs, static_cast<size_t>(shift));
        return result;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 BasicBigInt operator>>(BasicBigInt&& lhs, T shift) {
        lhs >>= shift;
        return std::move(lhs);
    }

    // --- 異質與整數比較運算子 ---

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator==(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        if (lhs.m_sign != rhs.m_sign) return false;
        if (lhs.m_size != rhs.m_size) return false;
        if (lhs.m_size == 0) return true;
        for (size_t i = 0; i < lhs.m_size; ++i) {
            if (lhs.m_data[i] != rhs.m_data[i]) return false;
        }
        return true;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator==(const BasicBigInt& lhs, T rhs) noexcept {
        return lhs == BasicBigInt(rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator==(T lhs, const BasicBigInt& rhs) noexcept {
        return BasicBigInt(lhs) == rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator!=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        return !(lhs == rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator!=(const BasicBigInt& lhs, T rhs) noexcept {
        return !(lhs == rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator!=(T lhs, const BasicBigInt& rhs) noexcept {
        return !(lhs == rhs);
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator<(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        if (lhs.m_sign < rhs.m_sign) return true;
        if (lhs.m_sign > rhs.m_sign) return false;
        if (lhs.m_sign == 0) return false;
        int cmp = detail::BigIntCore::compare_unsigned(
            lhs.m_data, lhs.m_size,
            rhs.m_data, rhs.m_size);
        return (lhs.m_sign > 0) ? (cmp < 0) : (cmp > 0);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator<(const BasicBigInt& lhs, T rhs) noexcept {
        return lhs < BasicBigInt(rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator<(T lhs, const BasicBigInt& rhs) noexcept {
        return BasicBigInt(lhs) < rhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator<=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        return !(rhs < lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator<=(const BasicBigInt& lhs, T rhs) noexcept {
        return !(rhs < lhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator<=(T lhs, const BasicBigInt& rhs) noexcept {
        return !(rhs < lhs);
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator>(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator>(const BasicBigInt& lhs, T rhs) noexcept {
        return rhs < lhs;
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator>(T lhs, const BasicBigInt& rhs) noexcept {
        return rhs < lhs;
    }

    template <size_t OtherLimbs>
    friend NUMERIC_CONSTEXPR_20 bool operator>=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept {
        return !(lhs < rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator>=(const BasicBigInt& lhs, T rhs) noexcept {
        return !(lhs < rhs);
    }

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    friend NUMERIC_CONSTEXPR_20 bool operator>=(T lhs, const BasicBigInt& rhs) noexcept {
        return !(lhs < rhs);
    }

    /// <summary>
    /// 輸出串流運算子。
    /// </summary>
    friend std::ostream& operator<<(std::ostream& os, const BasicBigInt& val) {
        os << val.to_string();
        return os;
    }
};

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(BasicBigInt<4>) == 64, "BasicBigInt<4> must be exactly 64 bytes (1 cache line) on 64-bit platforms!");
static_assert(sizeof(BasicBigInt<8>) == 96, "BasicBigInt<8> must be exactly 96 bytes on 64-bit platforms!");
#endif

namespace detail {

    template <size_t SboLimbs>
    NUMERIC_CONSTEXPR_20 BigIntConstantProxy::operator BasicBigInt<SboLimbs>() const noexcept {
        return BasicBigInt<SboLimbs>(value);
    }

    template <size_t SboLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt<SboLimbs> BigIntConstantProxy::operator()() const noexcept {
        return BasicBigInt<SboLimbs>(value);
    }

    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 bool operator==(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept {
        return other == p.value;
    }

    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 bool operator==(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept {
        return other == p.value;
    }

    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 bool operator!=(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept {
        return !(other == p.value);
    }

    template <size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 bool operator!=(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept {
        return !(other == p.value);
    }

} // namespace detail

/// <summary>
/// 雙目邏輯及運算子。
/// </summary>
template <size_t N1, size_t N2>
NUMERIC_CONSTEXPR_20 bool operator&&(const BasicBigInt<N1>& lhs, const BasicBigInt<N2>& rhs) noexcept {
    return static_cast<bool>(lhs) && static_cast<bool>(rhs);
}

/// <summary>
/// 雙目邏輯或運算子。
/// </summary>
template <size_t N1, size_t N2>
NUMERIC_CONSTEXPR_20 bool operator||(const BasicBigInt<N1>& lhs, const BasicBigInt<N2>& rhs) noexcept {
    return static_cast<bool>(lhs) || static_cast<bool>(rhs);
}

template <size_t N, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator&&(const BasicBigInt<N>& lhs, T rhs) noexcept {
    return static_cast<bool>(lhs) && (rhs != 0);
}

template <size_t N, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator&&(T lhs, const BasicBigInt<N>& rhs) noexcept {
    return (lhs != 0) && static_cast<bool>(rhs);
}

template <size_t N, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator||(const BasicBigInt<N>& lhs, T rhs) noexcept {
    return static_cast<bool>(lhs) || (rhs != 0);
}

template <size_t N, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bool operator||(T lhs, const BasicBigInt<N>& rhs) noexcept {
    return (lhs != 0) || static_cast<bool>(rhs);
}

} // namespace numeric

#if NUMERIC_HAS_STD_FORMAT
#include <format>

namespace std {

/// <summary>
/// std::formatter specialization for numeric::BasicBigInt<SboLimbs>.
/// </summary>
template <size_t SboLimbs, typename CharT>
struct formatter<numeric::BasicBigInt<SboLimbs>, CharT> {
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

    template <typename FormatContext>
    auto format(const numeric::BasicBigInt<SboLimbs>& val, FormatContext& ctx) const -> decltype(ctx.out()) {
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
/// std::hash specialization for numeric::BasicBigInt<SboLimbs>.
/// </summary>
template <size_t SboLimbs>
struct hash<numeric::BasicBigInt<SboLimbs>> {
    size_t operator()(const numeric::BasicBigInt<SboLimbs>& val) const noexcept {
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
using numeric::BigInt;
using numeric::BigInt256;
using numeric::BigInt512;
using numeric::BigInt1024;
#endif

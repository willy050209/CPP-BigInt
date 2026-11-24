// filepath: include/numeric/BigInt.hpp
/**
 * @file BigInt.hpp
 * @brief Arbitrary-precision integer facade class (BasicBigInt<SboLimbs>) for CPP-BigInt.
 *        Provides C++23 concepts-constrained constructors, trailing return types, std::expected monadic
 *        parsing/conversion APIs, SBO fast paths, and std::formatter / std::hash specializations.
 *        任意精度整數高階封裝類別 (BasicBigInt<SboLimbs>)。
 *        採用 C++23 概念約束、全域尾隨回傳型別、std::expected 單子錯誤處理、SBO 快速路徑與標準庫特化。
 */
#pragma once

#include "Config.hpp"
#include "detail/BigIntCore.hpp"
#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <functional>
#include <iostream>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

#if NUMERIC_HAS_STD_FORMAT
#  include <format>
#endif

namespace numeric {

/// <summary>
/// Strongly-typed recoverable error codes for non-throwing BigInt operations.
/// 供無例外 BigInt 介面使用之強型別可恢復錯誤列舉。
/// </summary>
enum class BigIntError : std::uint8_t {
    EmptyInput,
    InvalidFormat,
    DivisionByZero,
    NegativeInput,
    Overflow
};

template <std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
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
    /// Constant proxy structure supporting both property-style (bigint::zero) and call-style (bigint::zero()).
    /// 常數輔助結構，支援屬性形式與函式呼叫形式取得零或一之常數。
    /// </summary>
    struct BigIntConstantProxy final {
        std::int64_t value{0};

        constexpr explicit BigIntConstantProxy(const std::int64_t v) noexcept : value{v} {}

        constexpr explicit operator std::int64_t() const noexcept { return value; }
        constexpr explicit operator bool() const noexcept { return value != 0; }

        template <std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
        NUMERIC_CONSTEXPR_20 operator BasicBigInt<SboLimbs>() const noexcept;

        template <std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
        NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator()() const noexcept -> BasicBigInt<SboLimbs>;

        NUMERIC_NODISCARD friend constexpr auto operator==(const BigIntConstantProxy a, const BigIntConstantProxy b) noexcept -> bool {
            return a.value == b.value;
        }

        NUMERIC_NODISCARD friend constexpr auto operator!=(const BigIntConstantProxy a, const BigIntConstantProxy b) noexcept -> bool {
            return a.value != b.value;
        }

        template <std::integral T>
        NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator==(const BigIntConstantProxy p, const T other) noexcept -> bool {
            return p.value == static_cast<std::int64_t>(other);
        }

        template <std::integral T>
        NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator==(const T other, const BigIntConstantProxy p) noexcept -> bool {
            return static_cast<std::int64_t>(other) == p.value;
        }

        template <std::integral T>
        NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator!=(const BigIntConstantProxy p, const T other) noexcept -> bool {
            return p.value != static_cast<std::int64_t>(other);
        }

        template <std::integral T>
        NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator!=(const T other, const BigIntConstantProxy p) noexcept -> bool {
            return static_cast<std::int64_t>(other) != p.value;
        }

        template <std::size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 auto operator==(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept -> bool;

        template <std::size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 auto operator==(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept -> bool;

        template <std::size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 auto operator!=(BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept -> bool;

        template <std::size_t OtherLimbs>
        friend NUMERIC_CONSTEXPR_20 auto operator!=(const BasicBigInt<OtherLimbs>& other, BigIntConstantProxy p) noexcept -> bool;
    };

    /// <summary>
    /// Template container providing inline constant proxies zero and one.
    /// 提供內聯常數代理 zero 與 one 的樣板基底結構。
    /// </summary>
    template <typename T = void>
    struct BigIntConstants {
        static constexpr BigIntConstantProxy zero{0};
        static constexpr BigIntConstantProxy one{1};
    };

} // namespace detail

/// <summary>
/// Arbitrary-precision signed integer facade parameterized by compile-time inline SBO limb capacity.
/// Derives from non-templated BigIntBase to decouple heavy numerical kernels and avoid code bloat.
/// 任意精度有符號整數類別，採用 LLVM SmallVector 雙層解耦架構並支援編譯期 SBO 容量參數化。
/// </summary>
/// <typeparam name="SboLimbs">Static inline SBO limb capacity (default 4 = 256-bit). / 靜態 SBO 內聯肢數（預設 4）。</typeparam>
template <std::size_t SboLimbs>
class BasicBigInt : public BigIntBase, public detail::BigIntConstants<> {
private:
    static constexpr std::size_t ACTUAL_SBO{(SboLimbs > 0U) ? SboLimbs : 1U};
    alignas(std::uint64_t) std::array<std::uint64_t, ACTUAL_SBO> m_inline_storage{};

public:
    static NUMERIC_INLINE_VAR constexpr detail::BigIntConstantProxy zero{0};
    static NUMERIC_INLINE_VAR constexpr detail::BigIntConstantProxy one{1};

    /// <summary>
    /// Default constructor initializing value to 0 inside inline SBO buffer.
    /// 預設建構子：初始化數值為 0 並啟用 SBO 內聯緩衝區。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt() noexcept : BigIntBase{} {
        this->bind_inline(std::span<std::uint64_t>{m_inline_storage}.subspan(0U, SboLimbs));
    }

    /// <summary>
    /// Copy constructor (same SBO capacity). / 複製建構子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(const BasicBigInt& other) : BasicBigInt{} {
        this->assign_from(other);
    }

    /// <summary>
    /// Cross-capacity copy constructor. / 跨 SBO 容量複製建構子。
    /// </summary>
    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt(const BasicBigInt<OtherLimbs>& other) : BasicBigInt{} {
        this->assign_from(other);
    }

    /// <summary>
    /// Move constructor (same SBO capacity). / 移動建構子（同容量）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(BasicBigInt&& other) noexcept : BasicBigInt{} {
        this->move_from(std::move(other));
    }

    /// <summary>
    /// Cross-capacity move constructor. / 跨 SBO 容量移動建構子。
    /// </summary>
    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 BasicBigInt(BasicBigInt<OtherLimbs>&& other) noexcept : BasicBigInt{} {
        this->move_from(std::move(other));
    }

    NUMERIC_CONSTEXPR_20 auto operator=(const BasicBigInt& other) -> BasicBigInt& {
        this->assign_from(other);
        return *this;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator=(const BasicBigInt<OtherLimbs>& other) -> BasicBigInt& {
        this->assign_from(other);
        return *this;
    }

    NUMERIC_CONSTEXPR_20 auto operator=(BasicBigInt&& other) noexcept -> BasicBigInt& {
        this->move_from(std::move(other));
        return *this;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator=(BasicBigInt<OtherLimbs>&& other) noexcept -> BasicBigInt& {
        this->move_from(std::move(other));
        return *this;
    }

    NUMERIC_CONSTEXPR_20 auto operator=(const detail::BigIntConstantProxy proxy) noexcept -> BasicBigInt& {
        return *this = BasicBigInt{proxy.value};
    }

    NUMERIC_CONSTEXPR_20 ~BasicBigInt() = default;

    /// <summary>
    /// Constructs directly from a constant proxy (e.g. bigint::zero, bigint::one).
    /// 自常數代理物件（如 bigint::zero、bigint::one）直接建構。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BasicBigInt(const detail::BigIntConstantProxy proxy) noexcept
        : BasicBigInt{proxy.value} {}

    /// <summary>
    /// Explicit constructor adopting an internal BigIntStorage instance.
    /// 顯式從內部 BigIntStorage 轉移建構 BasicBigInt。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(detail::BigIntStorage storage) noexcept
        : BasicBigInt{} {
        this->move_from(std::move(storage));
    }

    /// <summary>
    /// Explicit constructor from boolean (true -> 1, false -> 0).
    /// 自布林值顯式建構（true 為 1，false 為 0）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(const bool b) noexcept : BasicBigInt{} {
        this->set_uint64(b ? 1ULL : 0ULL, b ? 1 : 0);
    }

    /// <summary>
    /// Value-preserving constructor from any signed integral type (excluding bool).
    /// 自任意有符號整數型別建構（保值隱式轉換）。
    /// </summary>
    template <std::signed_integral T>
        requires (!std::same_as<T, bool>)
    NUMERIC_CONSTEXPR_20 BasicBigInt(const T v) noexcept : BasicBigInt{} {
        const auto val{static_cast<std::int64_t>(v)};
        if (val < 0) {
            this->set_uint64(static_cast<std::uint64_t>(-(val + 1)) + 1ULL, -1);
        } else {
            this->set_uint64(static_cast<std::uint64_t>(val), val > 0 ? 1 : 0);
        }
    }

    /// <summary>
    /// Value-preserving constructor from any unsigned integral type (excluding bool).
    /// 自任意無符號整數型別建構（保值隱式轉換）。
    /// </summary>
    template <std::unsigned_integral T>
        requires (!std::same_as<T, bool>)
    NUMERIC_CONSTEXPR_20 BasicBigInt(const T v) noexcept : BasicBigInt{} {
        const auto val{static_cast<std::uint64_t>(v)};
        this->set_uint64(val, val > 0ULL ? 1 : 0);
    }

    /// <summary>
    /// Explicit constructor parsing a decimal numeric::string_view.
    /// 自十進位字串視圖顯式解析建構 BasicBigInt。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(const numeric::string_view sv) : BasicBigInt{} {
        detail::BigIntCore::from_string(*this, sv);
    }

    /// <summary>
    /// Explicit constructor parsing a null-terminated C string (throws on nullptr).
    /// 自以空字元結尾之 C 字串顯式解析建構（空指標即刻拋出例外）。
    /// </summary>
    explicit NUMERIC_CONSTEXPR_20 BasicBigInt(const char* const s) : BasicBigInt{} {
        if (s == nullptr) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("null string pointer"));
        }
        detail::BigIntCore::from_string(*this, numeric::string_view{s});
    }

    /// <summary>
    /// Explicit constructor parsing a std::string.
    /// 自 std::string 顯式解析建構 BasicBigInt。
    /// </summary>
    explicit BasicBigInt(const std::string& s) : BasicBigInt{} {
        detail::BigIntCore::from_string(*this, numeric::string_view{s});
    }

    /// <summary>
    /// Explicit constructor converting std::bitset&lt;N&gt; into non-negative BasicBigInt.
    /// 自 std::bitset&lt;N&gt; 顯式建構非負任意精度整數。
    /// </summary>
    template <std::size_t N>
    explicit BasicBigInt(const std::bitset<N>& bs) : BasicBigInt{} {
        if constexpr (N > 0U) {
            const std::size_t limb_cnt{(N + 63U) / 64U};
            this->resize(limb_cnt, 0ULL);
            bool any_bit{false};
            const std::span<std::uint64_t> out{this->as_span()};
            for (std::size_t w{0}; w < limb_cnt; ++w) {
                std::uint64_t val{0};
                const std::size_t bits_in_limb{(w == limb_cnt - 1U) ? (N - w * 64U) : 64U};
                for (std::size_t b{0}; b < bits_in_limb; ++b) {
                    if (bs.test(w * 64U + b)) {
                        val |= (std::uint64_t{1} << b);
                        any_bit = true;
                    }
                }
                out[w] = val;
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
    }

    /// <summary>
    /// Parses a decimal string view into BasicBigInt (throws std::invalid_argument on failure).
    /// 自十進位字串視圖解析 BasicBigInt（失敗拋出例外）。
    /// </summary>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto from_string(const numeric::string_view sv) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::from_string(res, sv);
        return res;
    }

    /// <summary>
    /// Monadic non-throwing decimal parser returning std::expected&lt;BasicBigInt, BigIntError&gt;.
    /// 無例外單子式十進位字串解析介面，回傳 std::expected&lt;BasicBigInt, BigIntError&gt;。
    /// </summary>
    /// <param name="sv">Input decimal string view. / 輸入十進位字串視圖。</param>
    /// <returns>Parsed BasicBigInt or BigIntError. / 解析結果或 BigIntError。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto try_from_string(
        const numeric::string_view sv) -> std::expected<BasicBigInt, BigIntError>
    {
        if (sv.empty()) {
            return std::unexpected{BigIntError::EmptyInput};
        }
        std::size_t idx{0};
        if (sv[0] == '-' || sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            return std::unexpected{BigIntError::InvalidFormat};
        }
        const bool all_digits{std::ranges::all_of(sv.substr(idx), [](const char ch) noexcept -> bool {
            return ch >= '0' && ch <= '9';
        })};
        if (!all_digits) {
            return std::unexpected{BigIntError::InvalidFormat};
        }
        BasicBigInt res;
        detail::BigIntCore::from_string(res, sv);
        return res;
    }

    /// <summary>
    /// Parses a binary string view (optional sign and 0b/0B prefix) into BasicBigInt.
    /// 自二進位字串視圖（支援選填正負號及 0b/0B 前綴）解析 BasicBigInt。
    /// </summary>
    NUMERIC_NODISCARD static auto from_binary_string(const numeric::string_view sv) -> BasicBigInt {
        const auto parsed{try_from_binary_string(sv)};
        if (!parsed) {
            if (parsed.error() == BigIntError::EmptyInput) {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty binary string"));
            }
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid binary string"));
        }
        return *parsed;
    }

    /// <summary>
    /// Monadic non-throwing binary parser returning std::expected&lt;BasicBigInt, BigIntError&gt;.
    /// 無例外單子式二進位字串解析介面，回傳 std::expected&lt;BasicBigInt, BigIntError&gt;。
    /// </summary>
    NUMERIC_NODISCARD static auto try_from_binary_string(
        const numeric::string_view sv) -> std::expected<BasicBigInt, BigIntError>
    {
        if (sv.empty()) {
            return std::unexpected{BigIntError::EmptyInput};
        }
        std::size_t idx{0};
        std::int8_t sign_val{1};
        if (sv[0] == '-') {
            sign_val = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            return std::unexpected{BigIntError::InvalidFormat};
        }
        if (idx + 1U < sv.size() && sv[idx] == '0' && (sv[idx + 1U] == 'b' || sv[idx + 1U] == 'B')) {
            idx += 2U;
        }
        if (idx == sv.size()) {
            return std::unexpected{BigIntError::InvalidFormat};
        }
        const bool all_bin{std::ranges::all_of(sv.substr(idx), [](const char ch) noexcept -> bool {
            return ch == '0' || ch == '1';
        })};
        if (!all_bin) {
            return std::unexpected{BigIntError::InvalidFormat};
        }
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            return BasicBigInt{0};
        }

        const std::size_t num_bits{sv.size() - idx};
        const std::size_t limb_cnt{(num_bits + 63U) / 64U};
        BasicBigInt res;
        res.resize(limb_cnt, 0ULL);
        const std::span<std::uint64_t> out{res.as_span()};
        for (std::size_t w{0}; w < limb_cnt; ++w) {
            std::uint64_t limb_val{0};
            const std::size_t bits_in_limb{(w == limb_cnt - 1U) ? (num_bits - w * 64U) : 64U};
            for (std::size_t b{0}; b < bits_in_limb; ++b) {
                const std::size_t char_pos{sv.size() - 1U - (w * 64U + b)};
                if (sv[char_pos] == '1') {
                    limb_val |= (std::uint64_t{1} << b);
                }
            }
            out[w] = limb_val;
        }
        res.m_sign = sign_val;
        res.normalize();
        return res;
    }

    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto storage() noexcept -> BigIntBase& {
        return *this;
    }

    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto storage() const noexcept -> const BigIntBase& {
        return *this;
    }

    explicit NUMERIC_CONSTEXPR_20 operator bool() const noexcept {
        return this->m_sign != 0;
    }

    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator!() const noexcept -> bool {
        return this->m_sign == 0;
    }

    /// <summary>
    /// Overflow-checked conversion to 64-bit signed integer returning std::expected.
    /// 具備溢位檢查之 64 位元有符號整數安全轉換介面。
    /// </summary>
    [[nodiscard]] NUMERIC_CONSTEXPR_20 auto try_to_int64() const noexcept -> std::expected<std::int64_t, BigIntError> {
        if (this->m_size == 0U || this->m_sign == 0) {
            return std::int64_t{0};
        }
        if (this->m_size > 1U) {
            return std::unexpected{BigIntError::Overflow};
        }
        const std::uint64_t mag{this->as_span()[0]};
        if (this->m_sign > 0) {
            if (mag > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                return std::unexpected{BigIntError::Overflow};
            }
            return static_cast<std::int64_t>(mag);
        }
        constexpr std::uint64_t abs_min{static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ULL};
        if (mag > abs_min) {
            return std::unexpected{BigIntError::Overflow};
        }
        if (mag == abs_min) {
            return std::numeric_limits<std::int64_t>::min();
        }
        return -static_cast<std::int64_t>(mag);
    }

    /// <summary>
    /// Overflow- and sign-checked conversion to 64-bit unsigned integer returning std::expected.
    /// 具備溢位與負號檢查之 64 位元無符號整數安全轉換介面。
    /// </summary>
    [[nodiscard]] NUMERIC_CONSTEXPR_20 auto try_to_uint64() const noexcept -> std::expected<std::uint64_t, BigIntError> {
        if (this->m_sign < 0) {
            return std::unexpected{BigIntError::NegativeInput};
        }
        if (this->m_size == 0U) {
            return std::uint64_t{0};
        }
        if (this->m_size > 1U) {
            return std::unexpected{BigIntError::Overflow};
        }
        return this->as_span()[0];
    }

    explicit NUMERIC_CONSTEXPR_20 operator std::int64_t() const noexcept {
        if (this->m_size == 0U) return 0;
        const std::uint64_t mag{this->as_span()[0]};
        if (this->m_sign < 0) {
            return -static_cast<std::int64_t>(mag);
        }
        return static_cast<std::int64_t>(mag);
    }

    explicit NUMERIC_CONSTEXPR_20 operator std::uint64_t() const noexcept {
        if (this->m_size == 0U) return 0ULL;
        return this->as_span()[0];
    }

    explicit NUMERIC_CONSTEXPR_20 operator std::int32_t() const noexcept {
        return static_cast<std::int32_t>(static_cast<std::int64_t>(*this));
    }

    explicit NUMERIC_CONSTEXPR_20 operator std::uint32_t() const noexcept {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(*this));
    }

    explicit operator double() const noexcept {
        if (this->m_size == 0U) return 0.0;
        double res{0.0};
        double base{1.0};
        constexpr double two_pow_64{18446744073709551616.0};
        for (const std::uint64_t limb : this->as_span()) {
            res += static_cast<double>(limb) * base;
            base *= two_pow_64;
        }
        return (this->m_sign < 0) ? -res : res;
    }

    /// <summary>
    /// Formats value into base-10 decimal string.
    /// 轉換為十進位字串表示。
    /// </summary>
    NUMERIC_NODISCARD auto to_string() const -> std::string {
        return detail::BigIntCore::to_string(*this);
    }

    /// <summary>
    /// Converts to std::bitset&lt;N&gt; using exact two's complement representation for negative numbers.
    /// 轉換為指定位元寬度之 std::bitset，負數採用標準二補數延伸。
    /// </summary>
    template <std::size_t N>
    NUMERIC_NODISCARD auto to_bitset() const -> std::bitset<N> {
        std::bitset<N> bs;
        if constexpr (N == 0U) {
            return bs;
        } else {
            const std::size_t limb_cnt{(N + 63U) / 64U};
            const std::span<const std::uint64_t> limbs_view{this->as_span()};
            if (this->m_sign >= 0) {
                for (std::size_t w{0}; w < limb_cnt; ++w) {
                    const std::uint64_t val{(w < limbs_view.size()) ? limbs_view[w] : 0ULL};
                    const std::size_t bits_in_limb{(w == limb_cnt - 1U) ? (N - w * 64U) : 64U};
                    for (std::size_t b{0}; b < bits_in_limb; ++b) {
                        if (((val >> b) & 1ULL) != 0ULL) {
                            bs.set(w * 64U + b);
                        }
                    }
                }
            } else {
                std::uint64_t carry{1ULL};
                for (std::size_t w{0}; w < limb_cnt; ++w) {
                    const std::uint64_t mag_w{(w < limbs_view.size()) ? limbs_view[w] : 0ULL};
                    const std::uint64_t inv_w{~mag_w};
                    const std::uint64_t val{inv_w + carry};
                    carry = (val < inv_w) ? 1ULL : 0ULL;
                    const std::size_t bits_in_limb{(w == limb_cnt - 1U) ? (N - w * 64U) : 64U};
                    for (std::size_t b{0}; b < bits_in_limb; ++b) {
                        if (((val >> b) & 1ULL) != 0ULL) {
                            bs.set(w * 64U + b);
                        }
                    }
                }
            }
            return bs;
        }
    }

    /// <summary>
    /// Formats value into base-2 binary string.
    /// 轉換為二進位字串表示。
    /// </summary>
    NUMERIC_NODISCARD auto to_binary_string() const -> std::string {
        if (this->m_sign == 0 || this->m_size == 0U) {
            return "0";
        }
        const std::span<const std::uint64_t> limbs_view{this->as_span()};
        const std::size_t high_limb_idx{limbs_view.size() - 1U};
        const std::uint64_t high_val{limbs_view[high_limb_idx]};
        const std::int32_t leading_zeros{detail::BigIntCore::clz64(high_val)};
        const std::int32_t bits_in_high{64 - leading_zeros};
        const std::size_t total_bits{high_limb_idx * 64U + static_cast<std::size_t>(bits_in_high)};

        std::string result{};
        result.reserve((this->m_sign < 0 ? 1U : 0U) + total_bits);
        if (this->m_sign < 0) {
            result.push_back('-');
        }
        for (std::int32_t b{bits_in_high - 1}; b >= 0; --b) {
            result.push_back(((high_val >> b) & 1ULL) != 0ULL ? '1' : '0');
        }
        for (std::size_t i{high_limb_idx}; i > 0U; --i) {
            const std::uint64_t val{limbs_view[i - 1U]};
            for (std::int32_t b{63}; b >= 0; --b) {
                result.push_back(((val >> b) & 1ULL) != 0ULL ? '1' : '0');
            }
        }
        return result;
    }

    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator+() const -> BasicBigInt {
        return *this;
    }

    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator-() && noexcept -> BasicBigInt {
        this->negate();
        return std::move(*this);
    }

    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator-() const & -> BasicBigInt {
        BasicBigInt res{*this};
        res.negate();
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto assign_negated(const BasicBigInt<OtherLimbs>& other) -> BasicBigInt& {
        this->assign_from(other);
        this->negate();
        return *this;
    }

    NUMERIC_CONSTEXPR_20 auto operator++() -> BasicBigInt& {
        *this += 1;
        return *this;
    }

    NUMERIC_CONSTEXPR_20 auto operator++(int) -> BasicBigInt {
        BasicBigInt tmp{*this};
        *this += 1;
        return tmp;
    }

    NUMERIC_CONSTEXPR_20 auto operator--() -> BasicBigInt& {
        *this -= 1;
        return *this;
    }

    NUMERIC_CONSTEXPR_20 auto operator--(int) -> BasicBigInt {
        BasicBigInt tmp{*this};
        *this -= 1;
        return tmp;
    }

    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator~() const -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::bitwise_not(res, *this);
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator+=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_sign > 0 && rhs.m_sign > 0 && this->m_size == 1U && rhs.m_size == 1U) {
            const std::uint8_t c{detail::BigIntCore::adc64(0, this->m_data[0], rhs.m_data[0], this->m_data[0])};
            if (c != 0U) {
                this->capacity_span()[1] = 1ULL;
                this->m_size = 2U;
            }
            return *this;
        }
        if (this->m_sign > 0 && rhs.m_sign > 0 && this->m_size == 2U && rhs.m_size == 2U) {
            std::uint8_t c{detail::BigIntCore::adc64(0, this->m_data[0], rhs.m_data[0], this->m_data[0])};
            c = detail::BigIntCore::adc64(c, this->m_data[1], rhs.m_data[1], this->m_data[1]);
            if (c != 0U) {
                this->capacity_span()[2] = 1ULL;
                this->m_size = 3U;
            }
            return *this;
        }
        detail::BigIntCore::add_signed(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator+=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        detail::BigIntCore::add_signed(*this, *this, tmp);
        return *this;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator-=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_sign > 0 && rhs.m_sign > 0 && this->m_size == 1U && rhs.m_size == 1U) {
            const std::uint64_t a0{this->as_span()[0]};
            const std::uint64_t b0{rhs.as_span()[0]};
            if (a0 > b0) {
                this->as_span()[0] = a0 - b0;
                return *this;
            }
            if (a0 < b0) {
                this->as_span()[0] = b0 - a0;
                this->m_sign = -1;
                return *this;
            }
            this->m_size = 0U;
            this->m_sign = 0;
            return *this;
        }
        if (this->m_sign > 0 && rhs.m_sign > 0 && this->m_size == 2U && rhs.m_size == 2U) {
            const std::uint64_t a0{this->as_span()[0]};
            const std::uint64_t a1{this->as_span()[1]};
            const std::uint64_t b0{rhs.as_span()[0]};
            const std::uint64_t b1{rhs.as_span()[1]};
            if (a1 > b1 || (a1 == b1 && a0 >= b0)) {
                const std::uint8_t borrow{detail::BigIntCore::sbb64(0, a0, b0, this->as_span()[0])};
                (void)detail::BigIntCore::sbb64(borrow, a1, b1, this->as_span()[1]);
                if (this->as_span()[1] == 0ULL) {
                    if (this->as_span()[0] == 0ULL) {
                        this->m_size = 0U;
                        this->m_sign = 0;
                    } else {
                        this->m_size = 1U;
                    }
                }
                return *this;
            }
        }
        detail::BigIntCore::sub_signed(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator-=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        detail::BigIntCore::sub_signed(*this, *this, tmp);
        return *this;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator*=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_size == 0U || this->m_sign == 0) {
            return *this;
        }
        if (rhs.m_size == 0U || rhs.m_sign == 0) {
            this->m_size = 0U;
            this->m_sign = 0;
            return *this;
        }
        if (rhs.m_size == 1U) {
            const std::uint64_t mult{rhs.as_span()[0]};
            const std::int8_t new_sign{static_cast<std::int8_t>(this->m_sign * rhs.m_sign)};
            if (this->m_capacity < this->m_size + 1U) {
                this->reserve(this->m_size + 1U);
            }
            const std::uint64_t carry{detail::BigIntCore::mul_1_raw(this->as_span(), this->as_span(), mult)};
            if (carry != 0ULL) {
                this->capacity_span()[this->m_size] = carry;
                this->m_size += 1U;
            }
            this->m_sign = new_sign;
            this->normalize();
            return *this;
        }
        const std::size_t total_len{this->m_size + rhs.m_size};
        if (total_len <= 16U) {
            std::array<std::uint64_t, 16> stack_buf{};
            detail::BigIntCore::mul_schoolbook_raw(stack_buf, this->as_span(), rhs.as_span());
            const std::int8_t new_sign{static_cast<std::int8_t>(this->m_sign * rhs.m_sign)};
            if (this->m_capacity < total_len) {
                this->reserve(total_len);
            }
            BigIntBase::copy_limbs(this->capacity_span(), std::span<const std::uint64_t>{stack_buf}.subspan(0U, total_len));
            this->m_size = static_cast<std::uint32_t>(total_len);
            this->m_sign = new_sign;
            this->normalize();
            return *this;
        }
        BasicBigInt tmp;
        detail::BigIntCore::mul_signed(tmp, *this, rhs);
        *this = std::move(tmp);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator*=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this *= tmp;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator/=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (rhs.m_size == 0U) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (this->m_size == 0U) return *this;
        if (rhs.m_size == 1U) {
            const std::uint64_t divisor{rhs.as_span()[0]};
            const std::int8_t new_sign{static_cast<std::int8_t>(this->m_sign * rhs.m_sign)};
            std::uint64_t rem{0};
            const std::span<std::uint64_t> d{this->as_span()};
            for (std::size_t i{this->m_size}; i > 0U; --i) {
                std::uint64_t next_rem{0};
                d[i - 1U] = detail::BigIntCore::div128_64(rem, d[i - 1U], divisor, next_rem);
                rem = next_rem;
            }
            this->m_sign = (this->m_size > 0U) ? new_sign : 0;
            this->normalize();
            return *this;
        }
        detail::BigIntCore::div_q_signed(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator/=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this /= tmp;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator%=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (rhs.m_size == 0U) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (this->m_size == 0U) return *this;
        if (rhs.m_size == 1U) {
            const std::uint64_t divisor{rhs.as_span()[0]};
            const std::int8_t orig_sign{this->m_sign};
            std::uint64_t rem{0};
            const std::span<const std::uint64_t> d{this->as_span()};
            for (std::size_t i{this->m_size}; i > 0U; --i) {
                std::uint64_t next_rem{0};
                (void)detail::BigIntCore::div128_64(rem, d[i - 1U], divisor, next_rem);
                rem = next_rem;
            }
            if (rem == 0ULL) {
                this->m_size = 0U;
                this->m_sign = 0;
            } else {
                this->set_uint64(rem, orig_sign);
            }
            return *this;
        }
        detail::BigIntCore::div_r_signed(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator%=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this %= tmp;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator&=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_sign > 0 && rhs.m_sign > 0) {
            std::size_t min_len{std::min<std::size_t>(this->m_size, rhs.m_size)};
            const std::span<std::uint64_t> d{this->as_span()};
            const std::span<const std::uint64_t> s{rhs.as_span()};
            if (min_len == 1U) {
                d[0] &= s[0];
                this->m_size = (d[0] != 0ULL) ? 1U : 0U;
                this->m_sign = (this->m_size != 0U) ? 1 : 0;
                return *this;
            }
            if (min_len == 2U) {
                d[0] &= s[0];
                d[1] &= s[1];
                this->m_size = (d[1] != 0ULL) ? 2U : ((d[0] != 0ULL) ? 1U : 0U);
                this->m_sign = (this->m_size != 0U) ? 1 : 0;
                return *this;
            }
            if (min_len == 4U) {
                d[0] &= s[0];
                d[1] &= s[1];
                d[2] &= s[2];
                d[3] &= s[3];
                std::size_t sz{4U};
                while (sz > 0U && d[sz - 1U] == 0ULL) --sz;
                this->m_size = static_cast<std::uint32_t>(sz);
                this->m_sign = (sz != 0U) ? 1 : 0;
                return *this;
            }
            std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
            const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
            std::size_t i{0};
            for (; i + 3U < min_len; i += 4U) {
                dp[i] &= sp[i];
                dp[i + 1U] &= sp[i + 1U];
                dp[i + 2U] &= sp[i + 2U];
                dp[i + 3U] &= sp[i + 3U];
            }
            for (; i < min_len; ++i) {
                dp[i] &= sp[i];
            }
            while (min_len > 0U && d[min_len - 1U] == 0ULL) --min_len;
            this->m_size = static_cast<std::uint32_t>(min_len);
            this->m_sign = (min_len != 0U) ? 1 : 0;
            return *this;
        }
        detail::BigIntCore::bitwise_and(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator&=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this &= tmp;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator|=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_sign == 0) {
            *this = rhs;
            return *this;
        }
        if (rhs.m_sign == 0) {
            return *this;
        }
        if (this->m_sign > 0 && rhs.m_sign > 0) {
            if (this->m_size >= rhs.m_size) {
                const std::span<std::uint64_t> d{this->as_span()};
                const std::span<const std::uint64_t> s{rhs.as_span()};
                if (rhs.m_size == 1U) {
                    d[0] |= s[0];
                    return *this;
                }
                if (rhs.m_size == 2U) {
                    d[0] |= s[0];
                    d[1] |= s[1];
                    return *this;
                }
                if (rhs.m_size == 4U) {
                    d[0] |= s[0];
                    d[1] |= s[1];
                    d[2] |= s[2];
                    d[3] |= s[3];
                    return *this;
                }
                std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
                const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
                std::size_t i{0};
                for (; i + 3U < rhs.m_size; i += 4U) {
                    dp[i] |= sp[i];
                    dp[i + 1U] |= sp[i + 1U];
                    dp[i + 2U] |= sp[i + 2U];
                    dp[i + 3U] |= sp[i + 3U];
                }
                for (; i < rhs.m_size; ++i) {
                    dp[i] |= sp[i];
                }
                return *this;
            }
            const std::size_t old_size{this->m_size};
            const std::size_t new_size{rhs.m_size};
            if (this->m_capacity < new_size) {
                this->reserve(new_size);
            }
            std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
            const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
            std::size_t i{0};
            for (; i + 3U < old_size; i += 4U) {
                dp[i] |= sp[i];
                dp[i + 1U] |= sp[i + 1U];
                dp[i + 2U] |= sp[i + 2U];
                dp[i + 3U] |= sp[i + 3U];
            }
            for (; i < old_size; ++i) {
                dp[i] |= sp[i];
            }
            BigIntBase::copy_limbs(this->capacity_span().subspan(old_size), rhs.as_span().subspan(old_size));
            this->m_size = static_cast<std::uint32_t>(new_size);
            return *this;
        }
        detail::BigIntCore::bitwise_or(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator|=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this |= tmp;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_CONSTEXPR_20 auto operator^=(const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt& {
        if (this->m_sign == 0) {
            *this = rhs;
            return *this;
        }
        if (rhs.m_sign == 0) {
            return *this;
        }
        if (this->m_sign > 0 && rhs.m_sign > 0) {
            if (this->m_size == rhs.m_size) {
                const std::span<std::uint64_t> d{this->as_span()};
                const std::span<const std::uint64_t> s{rhs.as_span()};
                if (this->m_size == 1U) {
                    d[0] ^= s[0];
                    this->m_size = (d[0] != 0ULL) ? 1U : 0U;
                    this->m_sign = (this->m_size != 0U) ? 1 : 0;
                    return *this;
                }
                if (this->m_size == 2U) {
                    d[0] ^= s[0];
                    d[1] ^= s[1];
                    this->m_size = (d[1] != 0ULL) ? 2U : ((d[0] != 0ULL) ? 1U : 0U);
                    this->m_sign = (this->m_size != 0U) ? 1 : 0;
                    return *this;
                }
                if (this->m_size == 4U) {
                    d[0] ^= s[0];
                    d[1] ^= s[1];
                    d[2] ^= s[2];
                    d[3] ^= s[3];
                    std::size_t sz{4U};
                    while (sz > 0U && d[sz - 1U] == 0ULL) --sz;
                    this->m_size = static_cast<std::uint32_t>(sz);
                    this->m_sign = (sz != 0U) ? 1 : 0;
                    return *this;
                }
                std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
                const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
                std::size_t i{0};
                for (; i + 3U < this->m_size; i += 4U) {
                    dp[i] ^= sp[i];
                    dp[i + 1U] ^= sp[i + 1U];
                    dp[i + 2U] ^= sp[i + 2U];
                    dp[i + 3U] ^= sp[i + 3U];
                }
                for (; i < this->m_size; ++i) {
                    dp[i] ^= sp[i];
                }
                std::size_t sz{this->m_size};
                while (sz > 0U && d[sz - 1U] == 0ULL) --sz;
                this->m_size = static_cast<std::uint32_t>(sz);
                this->m_sign = (sz != 0U) ? 1 : 0;
                return *this;
            }
            if (this->m_size > rhs.m_size) {
                std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
                const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
                std::size_t i{0};
                for (; i + 3U < rhs.m_size; i += 4U) {
                    dp[i] ^= sp[i];
                    dp[i + 1U] ^= sp[i + 1U];
                    dp[i + 2U] ^= sp[i + 2U];
                    dp[i + 3U] ^= sp[i + 3U];
                }
                for (; i < rhs.m_size; ++i) {
                    dp[i] ^= sp[i];
                }
                return *this;
            }
            const std::size_t old_size{this->m_size};
            const std::size_t new_size{rhs.m_size};
            if (this->m_capacity < new_size) {
                this->reserve(new_size);
            }
            std::uint64_t* NUMERIC_RESTRICT dp{this->m_data};
            const std::uint64_t* NUMERIC_RESTRICT sp{rhs.m_data};
            std::size_t i{0};
            for (; i + 3U < old_size; i += 4U) {
                dp[i] ^= sp[i];
                dp[i + 1U] ^= sp[i + 1U];
                dp[i + 2U] ^= sp[i + 2U];
                dp[i + 3U] ^= sp[i + 3U];
            }
            for (; i < old_size; ++i) {
                dp[i] ^= sp[i];
            }
            BigIntBase::copy_limbs(this->capacity_span().subspan(old_size), rhs.as_span().subspan(old_size));
            this->m_size = static_cast<std::uint32_t>(new_size);
            return *this;
        }
        detail::BigIntCore::bitwise_xor(*this, *this, rhs);
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator^=(const T rhs) -> BasicBigInt& {
        const BasicBigInt tmp{rhs};
        return *this ^= tmp;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator<<=(const T shift) -> BasicBigInt& {
        if constexpr (std::is_signed_v<T>) {
            if (shift < 0) {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
            }
        }
        detail::BigIntCore::shift_left(*this, *this, static_cast<std::size_t>(shift));
        return *this;
    }

    template <std::integral T>
    NUMERIC_CONSTEXPR_20 auto operator>>=(const T shift) -> BasicBigInt& {
        if constexpr (std::is_signed_v<T>) {
            if (shift < 0) {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
            }
        }
        detail::BigIntCore::shift_right(*this, *this, static_cast<std::size_t>(shift));
        return *this;
    }

    // --- Heterogeneous Binary Operators / 異質雙目運算子 ---

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator+(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res += rhs;
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator+(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        lhs += rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator+(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res += rhs;
        return res;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator+(BasicBigInt&& lhs, const T rhs) -> BasicBigInt {
        lhs += rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator+(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res += rhs;
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator-(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res -= rhs;
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator-(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        lhs -= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator-(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res -= rhs;
        return res;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator-(BasicBigInt&& lhs, const T rhs) -> BasicBigInt {
        lhs -= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator-(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        BasicBigInt res{lhs};
        res -= rhs;
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator*(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::mul_signed(res, lhs, rhs);
        return res;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator*(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs * tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator*(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp * rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator/(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::div_q_signed(res, lhs, rhs);
        return res;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator/(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs / tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator/(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp / rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator%(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::div_r_signed(res, lhs, rhs);
        return res;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator%(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs % tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator%(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp % rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator&(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::bitwise_and(res, lhs, rhs);
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator&(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        lhs &= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator&(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs & tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator&(BasicBigInt&& lhs, const T rhs) -> BasicBigInt {
        lhs &= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator&(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp & rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator|(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::bitwise_or(res, lhs, rhs);
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator|(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        lhs |= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator|(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs | tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator|(BasicBigInt&& lhs, const T rhs) -> BasicBigInt {
        lhs |= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator|(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp | rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator^(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        BasicBigInt res;
        detail::BigIntCore::bitwise_xor(res, lhs, rhs);
        return res;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator^(BasicBigInt&& lhs, const BasicBigInt<OtherLimbs>& rhs) -> BasicBigInt {
        lhs ^= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator^(const BasicBigInt& lhs, const T rhs) -> BasicBigInt {
        const BasicBigInt tmp{rhs};
        return lhs ^ tmp;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator^(BasicBigInt&& lhs, const T rhs) -> BasicBigInt {
        lhs ^= rhs;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator^(const T lhs, const BasicBigInt& rhs) -> BasicBigInt {
        const BasicBigInt tmp{lhs};
        return tmp ^ rhs;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<<(const BasicBigInt& lhs, const T shift) -> BasicBigInt {
        if constexpr (std::is_signed_v<T>) {
            if (shift < 0) {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
            }
        }
        BasicBigInt result;
        detail::BigIntCore::shift_left(result, lhs, static_cast<std::size_t>(shift));
        return result;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<<(BasicBigInt&& lhs, const T shift) -> BasicBigInt {
        lhs <<= shift;
        return std::move(lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>>(const BasicBigInt& lhs, const T shift) -> BasicBigInt {
        if constexpr (std::is_signed_v<T>) {
            if (shift < 0) {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("negative bit shift"));
            }
        }
        BasicBigInt result;
        detail::BigIntCore::shift_right(result, lhs, static_cast<std::size_t>(shift));
        return result;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>>(BasicBigInt&& lhs, const T shift) -> BasicBigInt {
        lhs >>= shift;
        return std::move(lhs);
    }

    // --- Comparison Operators / 比較運算子 ---

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator==(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        if (lhs.m_sign != rhs.m_sign || lhs.m_size != rhs.m_size) return false;
        const std::span<const std::uint64_t> a{lhs.as_span()};
        const std::span<const std::uint64_t> b{rhs.as_span()};
        if (a.empty()) return true;
        if (a.size() == 1U) return a[0] == b[0];
        if (a.size() == 2U) return a[0] == b[0] && a[1] == b[1];
        if (a.size() == 4U) {
            return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
        }
        return std::ranges::equal(a, b);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator==(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return lhs == BasicBigInt{rhs};
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator==(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return BasicBigInt{lhs} == rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator!=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        return !(lhs == rhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator!=(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return !(lhs == rhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator!=(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return !(lhs == rhs);
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        if (lhs.m_sign != rhs.m_sign) {
            return lhs.m_sign < rhs.m_sign;
        }
        if (lhs.m_sign == 0) return false;
        const std::span<const std::uint64_t> a{lhs.as_span()};
        const std::span<const std::uint64_t> b{rhs.as_span()};
        if (lhs.m_sign > 0) {
            if (a.size() != b.size()) return a.size() < b.size();
            if (a.size() == 1U) return a[0] < b[0];
            if (a.size() == 2U) {
                if (a[1] != b[1]) return a[1] < b[1];
                return a[0] < b[0];
            }
            if (a.size() == 4U) {
                if (a[3] != b[3]) return a[3] < b[3];
                if (a[2] != b[2]) return a[2] < b[2];
                if (a[1] != b[1]) return a[1] < b[1];
                return a[0] < b[0];
            }
            for (std::size_t i{a.size()}; i > 0U; --i) {
                if (a[i - 1U] != b[i - 1U]) return a[i - 1U] < b[i - 1U];
            }
            return false;
        }
        if (a.size() != b.size()) return a.size() > b.size();
        if (a.size() == 1U) return a[0] > b[0];
        if (a.size() == 2U) {
            if (a[1] != b[1]) return a[1] > b[1];
            return a[0] > b[0];
        }
        if (a.size() == 4U) {
            if (a[3] != b[3]) return a[3] > b[3];
            if (a[2] != b[2]) return a[2] > b[2];
            if (a[1] != b[1]) return a[1] > b[1];
            return a[0] > b[0];
        }
        for (std::size_t i{a.size()}; i > 0U; --i) {
            if (a[i - 1U] != b[i - 1U]) return a[i - 1U] > b[i - 1U];
        }
        return false;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return lhs < BasicBigInt{rhs};
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return BasicBigInt{lhs} < rhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        return !(rhs < lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<=(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return !(rhs < lhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator<=(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return !(rhs < lhs);
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        return rhs < lhs;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return rhs < lhs;
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return rhs < lhs;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>=(const BasicBigInt& lhs, const BasicBigInt<OtherLimbs>& rhs) noexcept -> bool {
        return !(lhs < rhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>=(const BasicBigInt& lhs, const T rhs) noexcept -> bool {
        return !(lhs < rhs);
    }

    template <std::integral T>
    NUMERIC_NODISCARD friend NUMERIC_CONSTEXPR_20 auto operator>=(const T lhs, const BasicBigInt& rhs) noexcept -> bool {
        return !(lhs < rhs);
    }

    friend auto operator<<(std::ostream& os, const BasicBigInt& val) -> std::ostream& {
        os << val.to_string();
        return os;
    }
};

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(BasicBigInt<4>) == 64, "BasicBigInt<4> must be exactly 64 bytes (1 cache line) on 64-bit platforms!");
static_assert(sizeof(BasicBigInt<8>) == 96, "BasicBigInt<8> must be exactly 96 bytes on 64-bit platforms!");
#endif

namespace detail {

    template <std::size_t SboLimbs>
    NUMERIC_CONSTEXPR_20 BigIntConstantProxy::operator BasicBigInt<SboLimbs>() const noexcept {
        return BasicBigInt<SboLimbs>{value};
    }

    template <std::size_t SboLimbs>
    NUMERIC_CONSTEXPR_20 auto BigIntConstantProxy::operator()() const noexcept -> BasicBigInt<SboLimbs> {
        return BasicBigInt<SboLimbs>{value};
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator==(const BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept -> bool {
        return other == p.value;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator==(const BasicBigInt<OtherLimbs>& other, const BigIntConstantProxy p) noexcept -> bool {
        return other == p.value;
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator!=(const BigIntConstantProxy p, const BasicBigInt<OtherLimbs>& other) noexcept -> bool {
        return !(other == p.value);
    }

    template <std::size_t OtherLimbs>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator!=(const BasicBigInt<OtherLimbs>& other, const BigIntConstantProxy p) noexcept -> bool {
        return !(other == p.value);
    }

} // namespace detail

template <std::size_t N1, std::size_t N2>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator&&(const BasicBigInt<N1>& lhs, const BasicBigInt<N2>& rhs) noexcept -> bool {
    return static_cast<bool>(lhs) && static_cast<bool>(rhs);
}

template <std::size_t N1, std::size_t N2>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator||(const BasicBigInt<N1>& lhs, const BasicBigInt<N2>& rhs) noexcept -> bool {
    return static_cast<bool>(lhs) || static_cast<bool>(rhs);
}

template <std::size_t N, std::integral T>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator&&(const BasicBigInt<N>& lhs, const T rhs) noexcept -> bool {
    return static_cast<bool>(lhs) && (rhs != 0);
}

template <std::size_t N, std::integral T>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator&&(const T lhs, const BasicBigInt<N>& rhs) noexcept -> bool {
    return (lhs != 0) && static_cast<bool>(rhs);
}

template <std::size_t N, std::integral T>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator||(const BasicBigInt<N>& lhs, const T rhs) noexcept -> bool {
    return static_cast<bool>(lhs) || (rhs != 0);
}

template <std::size_t N, std::integral T>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto operator||(const T lhs, const BasicBigInt<N>& rhs) noexcept -> bool {
    return (lhs != 0) || static_cast<bool>(rhs);
}

} // namespace numeric

#if NUMERIC_HAS_STD_FORMAT
namespace std {

/// <summary>
/// std::formatter specialization for numeric::BasicBigInt&lt;SboLimbs&gt;.
/// </summary>
template <std::size_t SboLimbs, typename CharT>
struct formatter<numeric::BasicBigInt<SboLimbs>, CharT> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
        auto it{ctx.begin()};
        const auto end{ctx.end()};
        if (it != end && *it == ':') {
            ++it;
        }
        if (it != end && *it != '}') {
            throw std::format_error{"invalid format specifier for bigint"};
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const numeric::BasicBigInt<SboLimbs>& val, FormatContext& ctx) const -> decltype(ctx.out()) {
        const std::string s{val.to_string()};
        auto it{ctx.out()};
        for (const char c : s) {
            *it++ = static_cast<CharT>(c);
        }
        return it;
    }
};

} // namespace std
#endif // NUMERIC_HAS_STD_FORMAT

namespace std {

/// <summary>
/// std::hash specialization for numeric::BasicBigInt&lt;SboLimbs&gt; using FNV-1a + MurmurHash avalanche.
/// </summary>
template <std::size_t SboLimbs>
struct hash<numeric::BasicBigInt<SboLimbs>> {
    NUMERIC_NODISCARD auto operator()(const numeric::BasicBigInt<SboLimbs>& val) const noexcept -> std::size_t {
        if (val.sign() == 0) {
            return 0U;
        }
        std::uint64_t h{14695981039346656037ULL};
        constexpr std::uint64_t fnv_prime{1099511628211ULL};

        const std::uint64_t s{static_cast<std::uint64_t>(val.sign() < 0 ? 1ULL : 2ULL)};
        h ^= s;
        h *= fnv_prime;

        for (const std::uint64_t limb : val.as_span()) {
            h ^= limb;
            h *= fnv_prime;
        }

        h ^= h >> 33U;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33U;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33U;

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        return static_cast<std::size_t>(h);
#else
        return static_cast<std::size_t>(h ^ (h >> 32U));
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

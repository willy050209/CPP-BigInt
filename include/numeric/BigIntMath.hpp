// filepath: include/numeric/BigIntMath.hpp
/**
 * @file BigIntMath.hpp
 * @brief High-precision number-theoretic and mathematical functions for numeric::BasicBigInt.
 *        Implements tail-recursive GCD and exponentiation, integer square/cube roots, LCM,
 *        std::expected non-throwing try_isqrt, and std:: namespace specializations.
 *        高精度數論與數學運算函式庫（支援 numeric:: 與 std:: 命名空間）。
 *        採用尾遞迴實作最大公因數與快速冪，並提供無例外 std::expected 安全平方根介面。
 */
#pragma once

#include "BigInt.hpp"
#include "Config.hpp"
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <utility>

namespace numeric {

namespace detail {

    /// <summary>
    /// Tail-recursive Euclidean GCD helper operating purely in tail position.
    /// 處於純尾位置之歐幾里得最大公因數尾遞迴輔助函式。
    /// </summary>
    template <std::size_t N>
    NUMERIC_NODISCARD inline auto gcd_tail(BasicBigInt<N> a, BasicBigInt<N> b) -> BasicBigInt<N> {
        if (b == 0) {
            return a;
        }
        BasicBigInt<N> rem{a % b};
        return gcd_tail(std::move(b), std::move(rem));
    }

    /// <summary>
    /// Tail-recursive exponentiation with accumulator for unsigned integral exponents.
    /// 帶累加器之無符號整數快速冪尾遞迴輔助函式。
    /// </summary>
    template <std::size_t N>
    NUMERIC_NODISCARD inline auto pow_tail(
        BasicBigInt<N> base,
        const std::uint64_t exp,
        BasicBigInt<N> acc) -> BasicBigInt<N>
    {
        if (exp == 0ULL) {
            return acc;
        }
        if ((exp & 1ULL) != 0ULL) {
            acc *= base;
        }
        const std::uint64_t next_exp{exp >> 1U};
        if (next_exp == 0ULL) {
            return acc;
        }
        base *= base;
        return pow_tail(std::move(base), next_exp, std::move(acc));
    }

} // namespace detail

/// <summary>
/// Computes the absolute value of BasicBigInt.
/// 計算 BasicBigInt 之絕對值。
/// </summary>
/// <param name="x">Input integer. / 輸入整數。</param>
/// <returns>Absolute value. / 絕對值結果。</returns>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto abs(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N> {
    return (x.sign() < 0) ? -x : x;
}

/// <summary>
/// Monadic non-throwing integer square root returning std::expected&lt;BasicBigInt&lt;N&gt;, BigIntError&gt;.
/// 無例外單子式整數平方根計算，負數輸入時回傳 BigIntError::NegativeInput。
/// </summary>
/// <param name="x">Non-negative input integer. / 非負輸入整數。</param>
/// <returns>Floor integer square root or BigIntError. / 整數平方根或錯誤狀態。</returns>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
[[nodiscard]] inline auto try_isqrt(const BasicBigInt<N>& x) -> std::expected<BasicBigInt<N>, BigIntError> {
    if (x.sign() < 0) {
        return std::unexpected{BigIntError::NegativeInput};
    }
    if (x == 0) {
        return BasicBigInt<N>{0};
    }
    if (x == 1) {
        return BasicBigInt<N>{1};
    }

    const std::size_t count{x.limb_count()};
    const std::uint64_t high_limb{x.as_span()[count - 1U]};
    const std::int32_t high_bits{64 - static_cast<std::int32_t>(std::countl_zero(high_limb))};
    const std::size_t bit_len{(count - 1U) * 64U + static_cast<std::size_t>(high_bits)};
    const std::size_t shift{(bit_len + 1U) / 2U};

    BasicBigInt<N> x0{BasicBigInt<N>{1} << shift};
    BasicBigInt<N> x1{(x0 + x / x0) >> 1};
    while (x1 < x0) {
        x0 = std::move(x1);
        x1 = (x0 + x / x0) >> 1;
    }
    while (x0 * x0 > x) {
        --x0;
    }
    return x0;
}

/// <summary>
/// Computes integer floor square root: greatest r such that r^2 &lt;= x.
/// 計算 BasicBigInt 之整數平方根，回傳滿足 r^2 &lt;= x 之最大整數。
/// </summary>
/// <param name="x">Non-negative input integer. / 非負輸入整數。</param>
/// <returns>Integer square root. / 整數平方根。</returns>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto isqrt(const BasicBigInt<N>& x) -> BasicBigInt<N> {
    auto res{try_isqrt<N>(x)};
    if (!res) {
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("isqrt: square root of negative bigint"));
    }
    return std::move(*res);
}

/// <summary>
/// Alias for isqrt(). / isqrt() 之別名。
/// </summary>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto sqrt(const BasicBigInt<N>& x) -> BasicBigInt<N> {
    return isqrt<N>(x);
}

/// <summary>
/// Computes integer cube root supporting negative operands as an odd function.
/// 計算 BasicBigInt 之整數立方根（支援負數奇函數語意）。
/// </summary>
/// <param name="x">Input integer. / 輸入整數。</param>
/// <returns>Integer cube root. / 整數立方根。</returns>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto icbrt(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N> {
    if (x == 0) {
        return BasicBigInt<N>{0};
    }
    if (x.sign() < 0) {
        return -icbrt(-x);
    }
    if (x == 1) {
        return BasicBigInt<N>{1};
    }

    const std::size_t count{x.limb_count()};
    const std::uint64_t high_limb{x.as_span()[count - 1U]};
    const std::int32_t high_bits{64 - static_cast<std::int32_t>(std::countl_zero(high_limb))};
    const std::size_t bit_len{(count - 1U) * 64U + static_cast<std::size_t>(high_bits)};
    const std::size_t shift{(bit_len + 2U) / 3U};

    BasicBigInt<N> x0{BasicBigInt<N>{1} << shift};
    BasicBigInt<N> x1{(x0 * 2 + x / (x0 * x0)) / 3};
    while (x1 < x0) {
        x0 = std::move(x1);
        x1 = (x0 * 2 + x / (x0 * x0)) / 3;
    }
    while (x0 * x0 * x0 > x) {
        --x0;
    }
    while ((x0 + 1) * (x0 + 1) * (x0 + 1) <= x) {
        ++x0;
    }
    return x0;
}

/// <summary>
/// Alias for icbrt(). / icbrt() 之別名。
/// </summary>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto cbrt(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N> {
    return icbrt<N>(x);
}

/// <summary>
/// Fast binary exponentiation using tail-recursive accumulator pattern.
/// 採用尾遞迴累加器模式計算無符號整數指數之快速冪。
/// </summary>
/// <param name="base">Base value. / 底數。</param>
/// <param name="exp">Unsigned integral exponent. / 無符號整數指數。</param>
/// <returns>Power result. / 冪次結果。</returns>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS, std::unsigned_integral ExpT = std::uint32_t>
NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N>& base, const ExpT exp) -> BasicBigInt<N> {
    if (exp == 0) {
        return BasicBigInt<N>{1};
    }
    if (base == 0) {
        return BasicBigInt<N>{0};
    }
    if (base == 1) {
        return BasicBigInt<N>{1};
    }
    if (base == -1) {
        return ((exp & ExpT{1}) != 0) ? BasicBigInt<N>{-1} : BasicBigInt<N>{1};
    }
    return detail::pow_tail<N>(base, static_cast<std::uint64_t>(exp), BasicBigInt<N>{1});
}

/// <summary>
/// Fast binary exponentiation with signed integer exponent (convenience overload for literals).
/// 支援有符號整數指數之快速冪多載（負指數時非 1/-1 將拋出例外）。
/// </summary>
template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS, std::signed_integral ExpT>
    requires (!std::same_as<ExpT, bool>)
NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N>& base, const ExpT exp) -> BasicBigInt<N> {
    if (exp < 0) {
        if (base == 1) return BasicBigInt<N>{1};
        if (base == -1) return ((exp % 2) != 0) ? BasicBigInt<N>{-1} : BasicBigInt<N>{1};
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("pow: negative exponent in bigint pow"));
    }
    return pow<N>(base, static_cast<std::uint64_t>(exp));
}

/// <summary>
/// Exponentiation supporting arbitrary-precision BasicBigInt exponents.
/// 支援 BasicBigInt 任意精度指數之冪次計算。
/// </summary>
template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N1>& base, const BasicBigInt<N2>& exp) -> BasicBigInt<N1> {
    if (exp.sign() < 0) {
        if (base == 1) return BasicBigInt<N1>{1};
        if (base == -1) return (exp % 2 != 0) ? BasicBigInt<N1>{-1} : BasicBigInt<N1>{1};
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("pow: negative exponent in bigint pow"));
    }
    if (exp == 0) return BasicBigInt<N1>{1};
    if (base == 0) return BasicBigInt<N1>{0};
    if (base == 1) return BasicBigInt<N1>{1};
    if (base == -1) return (exp % 2 != 0) ? BasicBigInt<N1>{-1} : BasicBigInt<N1>{1};

    BasicBigInt<N1> res{1};
    BasicBigInt<N1> b{base};
    BasicBigInt<N2> e{exp};
    while (e > 0) {
        if (e % 2 != 0) {
            res *= b;
        }
        e >>= 1;
        if (e > 0) {
            b *= b;
        }
    }
    return res;
}

/// <summary>
/// Computes non-negative Greatest Common Divisor (GCD) via tail recursion.
/// 透過純尾遞迴計算兩 BasicBigInt 之非負最大公因數 (GCD)。
/// </summary>
template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto gcd(BasicBigInt<N1> a, const BasicBigInt<N2>& b) -> BasicBigInt<N1> {
    BasicBigInt<N1> abs_a{(a.sign() < 0) ? -std::move(a) : std::move(a)};
    BasicBigInt<N1> abs_b{(b.sign() < 0) ? BasicBigInt<N1>{-b} : BasicBigInt<N1>{b}};
    return detail::gcd_tail<N1>(std::move(abs_a), std::move(abs_b));
}

/// <summary>
/// Computes Least Common Multiple (LCM) using divide-before-multiply overflow avoidance.
/// 計算兩 BasicBigInt 之最小公倍數 (LCM)，採除後乘演算法防止中間值不必要膨脹。
/// </summary>
template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline auto lcm(const BasicBigInt<N1>& a, const BasicBigInt<N2>& b) -> BasicBigInt<N1> {
    if (a == 0 || b == 0) {
        return BasicBigInt<N1>{0};
    }
    const BasicBigInt<N1> g{gcd(a, b)};
    const BasicBigInt<N1> abs_a{(a.sign() < 0) ? -a : a};
    const BasicBigInt<N1> abs_b{(b.sign() < 0) ? BasicBigInt<N1>{-b} : BasicBigInt<N1>{b}};
    return (abs_a / g) * abs_b;
}

} // namespace numeric

namespace std {

template <std::size_t N>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto abs(const numeric::BasicBigInt<N>& x) noexcept -> numeric::BasicBigInt<N> {
    return numeric::abs(x);
}

template <std::size_t N>
NUMERIC_NODISCARD inline auto sqrt(const numeric::BasicBigInt<N>& x) -> numeric::BasicBigInt<N> {
    return numeric::sqrt(x);
}

template <std::size_t N>
NUMERIC_NODISCARD inline auto isqrt(const numeric::BasicBigInt<N>& x) -> numeric::BasicBigInt<N> {
    return numeric::isqrt(x);
}

template <std::size_t N>
NUMERIC_NODISCARD inline auto cbrt(const numeric::BasicBigInt<N>& x) noexcept -> numeric::BasicBigInt<N> {
    return numeric::cbrt(x);
}

template <std::size_t N>
NUMERIC_NODISCARD inline auto icbrt(const numeric::BasicBigInt<N>& x) noexcept -> numeric::BasicBigInt<N> {
    return numeric::icbrt(x);
}

template <std::size_t N, std::integral ExpT>
NUMERIC_NODISCARD inline auto pow(const numeric::BasicBigInt<N>& base, const ExpT exp) -> numeric::BasicBigInt<N> {
    return numeric::pow(base, exp);
}

template <std::size_t N1, std::size_t N2>
NUMERIC_NODISCARD inline auto pow(const numeric::BasicBigInt<N1>& base, const numeric::BasicBigInt<N2>& exp) -> numeric::BasicBigInt<N1> {
    return numeric::pow(base, exp);
}

template <std::size_t N1, std::size_t N2>
NUMERIC_NODISCARD inline auto gcd(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) -> numeric::BasicBigInt<N1> {
    return numeric::gcd(a, b);
}

template <std::size_t N1, std::size_t N2>
NUMERIC_NODISCARD inline auto lcm(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) -> numeric::BasicBigInt<N1> {
    return numeric::lcm(a, b);
}

} // namespace std

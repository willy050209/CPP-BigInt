#pragma once

// BigIntMath.hpp
// High-precision mathematical functions for numeric::BasicBigInt.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "Config.hpp"
#include "BigInt.hpp"
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace numeric {

/// <summary>
/// 計算 BasicBigInt 之絕對值。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>絕對值結果</returns>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 BasicBigInt<N> abs(const BasicBigInt<N>& x) noexcept {
    return (x.sign() < 0) ? -x : x;
}

/// <summary>
/// 計算 BasicBigInt 之整數平方根，回傳最大滿足 r^2 &lt;= x 之整數。
/// </summary>
/// <param name="x">非負輸入整數</param>
/// <returns>整數平方根</returns>
/// <exception cref="std::invalid_argument">若 x 為負數時拋出</exception>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N> isqrt(const BasicBigInt<N>& x) {
    if (x.sign() < 0) {
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("isqrt: square root of negative bigint"));
    }
    if (x == 0) {
        return BasicBigInt<N>(0);
    }
    if (x == 1) {
        return BasicBigInt<N>(1);
    }

    size_t count = x.limb_count();
    const uint64_t* limbs = x.limbs();
    uint64_t high_limb = limbs[count - 1];
    int high_bits = 0;
    while (high_limb > 0) {
        high_bits++;
        high_limb >>= 1;
    }
    size_t bit_len = (count - 1) * 64 + static_cast<size_t>(high_bits);
    size_t shift = (bit_len + 1) / 2;

    BasicBigInt<N> x0 = BasicBigInt<N>(1) << shift;
    BasicBigInt<N> x1 = (x0 + x / x0) >> 1;
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
/// 計算 BasicBigInt 之整數平方根（isqrt 之別名）。
/// </summary>
/// <param name="x">非負輸入整數</param>
/// <returns>整數平方根</returns>
/// <exception cref="std::invalid_argument">若 x 為負數時拋出</exception>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N> sqrt(const BasicBigInt<N>& x) {
    return isqrt<N>(x);
}

/// <summary>
/// 計算 BasicBigInt 之整數立方根，回傳整數立方根（奇函數，支援負數）。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>整數立方根</returns>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N> icbrt(const BasicBigInt<N>& x) noexcept {
    if (x == 0) {
        return BasicBigInt<N>(0);
    }
    if (x.sign() < 0) {
        return -icbrt(-x);
    }
    if (x == 1) {
        return BasicBigInt<N>(1);
    }

    size_t count = x.limb_count();
    const uint64_t* limbs = x.limbs();
    uint64_t high_limb = limbs[count - 1];
    int high_bits = 0;
    while (high_limb > 0) {
        high_bits++;
        high_limb >>= 1;
    }
    size_t bit_len = (count - 1) * 64 + static_cast<size_t>(high_bits);
    size_t shift = (bit_len + 2) / 3;

    BasicBigInt<N> x0 = BasicBigInt<N>(1) << shift;
    BasicBigInt<N> x1 = (x0 * 2 + x / (x0 * x0)) / 3;
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
/// 計算 BasicBigInt 之整數立方根（icbrt 之別名）。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>整數立方根</returns>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N> cbrt(const BasicBigInt<N>& x) noexcept {
    return icbrt<N>(x);
}

/// <summary>
/// 計算 BasicBigInt 之整數非負次方冪（快速冪演算法）。
/// </summary>
/// <param name="base">底數</param>
/// <param name="exp">無符號指數</param>
/// <returns>冪次結果</returns>
template <size_t N = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N> pow(const BasicBigInt<N>& base, unsigned int exp) {
    if (exp == 0) {
        return BasicBigInt<N>(1);
    }
    if (base == 0) {
        return BasicBigInt<N>(0);
    }
    if (base == 1) {
        return BasicBigInt<N>(1);
    }
    if (base == -1) {
        return (exp & 1u) ? BasicBigInt<N>(-1) : BasicBigInt<N>(1);
    }

    BasicBigInt<N> res(1);
    BasicBigInt<N> b = base;
    while (exp > 0) {
        if (exp & 1u) {
            res *= b;
        }
        exp >>= 1;
        if (exp > 0) {
            b *= b;
        }
    }
    return res;
}

/// <summary>
/// 計算 BasicBigInt 之整數冪（支援 BasicBigInt 指數）。
/// </summary>
/// <param name="base">底數</param>
/// <param name="exp">指數</param>
/// <returns>冪次結果</returns>
/// <exception cref="std::invalid_argument">若指數為負且底數非 1 或 -1 時拋出</exception>
template <size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N1> pow(const BasicBigInt<N1>& base, const BasicBigInt<N2>& exp) {
    if (exp.sign() < 0) {
        if (base == 1) {
            return BasicBigInt<N1>(1);
        }
        if (base == -1) {
            return (exp % 2 != 0) ? BasicBigInt<N1>(-1) : BasicBigInt<N1>(1);
        }
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("pow: negative exponent in bigint pow"));
    }
    if (exp == 0) {
        return BasicBigInt<N1>(1);
    }
    if (base == 0) {
        return BasicBigInt<N1>(0);
    }
    if (base == 1) {
        return BasicBigInt<N1>(1);
    }
    if (base == -1) {
        return (exp % 2 != 0) ? BasicBigInt<N1>(-1) : BasicBigInt<N1>(1);
    }

    BasicBigInt<N1> res(1);
    BasicBigInt<N1> b = base;
    BasicBigInt<N2> e = exp;
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
/// 計算兩 BasicBigInt 之最大公因數 (Greatest Common Divisor)。
/// </summary>
/// <param name="a">第一個整數</param>
/// <param name="b">第二個整數</param>
/// <returns>最大公因數（恆為非負）</returns>
template <size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N1> gcd(BasicBigInt<N1> a, BasicBigInt<N2> b) {
    a = (a.sign() < 0) ? -a : a;
    BasicBigInt<N1> b_copy(b);
    b_copy = (b_copy.sign() < 0) ? -b_copy : b_copy;
    while (b_copy != 0) {
        BasicBigInt<N1> r = a % b_copy;
        a = std::move(b_copy);
        b_copy = std::move(r);
    }
    return a;
}

/// <summary>
/// 計算兩 BasicBigInt 之最小公倍數 (Least Common Multiple)。
/// </summary>
/// <param name="a">第一個整數</param>
/// <param name="b">第二個整數</param>
/// <returns>最小公倍數（恆為非負）</returns>
template <size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
NUMERIC_NODISCARD inline BasicBigInt<N1> lcm(const BasicBigInt<N1>& a, const BasicBigInt<N2>& b) {
    if (a == 0 || b == 0) {
        return BasicBigInt<N1>(0);
    }
    BasicBigInt<N1> g = gcd(a, b);
    BasicBigInt<N1> abs_a = (a.sign() < 0) ? -a : a;
    BasicBigInt<N1> abs_b(b);
    abs_b = (abs_b.sign() < 0) ? -abs_b : abs_b;
    return (abs_a / g) * abs_b;
}

} // namespace numeric

namespace std {

template <size_t N>
NUMERIC_CONSTEXPR_20 numeric::BasicBigInt<N> abs(const numeric::BasicBigInt<N>& x) noexcept {
    return numeric::abs(x);
}

template <size_t N>
inline numeric::BasicBigInt<N> sqrt(const numeric::BasicBigInt<N>& x) {
    return numeric::sqrt(x);
}

template <size_t N>
inline numeric::BasicBigInt<N> isqrt(const numeric::BasicBigInt<N>& x) {
    return numeric::isqrt(x);
}

template <size_t N>
inline numeric::BasicBigInt<N> cbrt(const numeric::BasicBigInt<N>& x) noexcept {
    return numeric::cbrt(x);
}

template <size_t N>
inline numeric::BasicBigInt<N> icbrt(const numeric::BasicBigInt<N>& x) noexcept {
    return numeric::icbrt(x);
}

template <size_t N>
inline numeric::BasicBigInt<N> pow(const numeric::BasicBigInt<N>& base, unsigned int exp) {
    return numeric::pow(base, exp);
}

template <size_t N1, size_t N2>
inline numeric::BasicBigInt<N1> pow(const numeric::BasicBigInt<N1>& base, const numeric::BasicBigInt<N2>& exp) {
    return numeric::pow(base, exp);
}

template <size_t N1, size_t N2>
inline numeric::BasicBigInt<N1> gcd(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) {
    return numeric::gcd(a, b);
}

template <size_t N1, size_t N2>
inline numeric::BasicBigInt<N1> lcm(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) {
    return numeric::lcm(a, b);
}

} // namespace std

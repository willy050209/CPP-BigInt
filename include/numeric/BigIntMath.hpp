#pragma once

// BigIntMath.hpp
// High-precision mathematical functions for numeric::bigint.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "Config.hpp"
#include "BigInt.hpp"
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace numeric {

/// <summary>
/// 計算 bigint 之絕對值。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>絕對值結果</returns>
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 inline bigint abs(const bigint& x) noexcept {
    return (x.sign() < 0) ? -x : x;
}

/// <summary>
/// 計算 bigint 之整數平方根，回傳最大滿足 r^2 &lt;= x 之整數。
/// </summary>
/// <param name="x">非負輸入整數</param>
/// <returns>整數平方根</returns>
/// <exception cref="std::invalid_argument">若 x 為負數時拋出</exception>
NUMERIC_NODISCARD inline bigint isqrt(const bigint& x) {
    if (x.sign() < 0) {
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("isqrt: square root of negative bigint"));
    }
    if (x == 0) {
        return bigint(0);
    }
    if (x == 1) {
        return bigint(1);
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

    bigint x0 = bigint(1) << shift;
    bigint x1 = (x0 + x / x0) >> 1;
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
/// 計算 bigint 之整數平方根（isqrt 之別名）。
/// </summary>
/// <param name="x">非負輸入整數</param>
/// <returns>整數平方根</returns>
/// <exception cref="std::invalid_argument">若 x 為負數時拋出</exception>
NUMERIC_NODISCARD inline bigint sqrt(const bigint& x) {
    return isqrt(x);
}

/// <summary>
/// 計算 bigint 之整數立方根，回傳整數立方根（奇函數，支援負數）。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>整數立方根</returns>
NUMERIC_NODISCARD inline bigint icbrt(const bigint& x) noexcept {
    if (x == 0) {
        return bigint(0);
    }
    if (x.sign() < 0) {
        return -icbrt(-x);
    }
    if (x == 1) {
        return bigint(1);
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

    bigint x0 = bigint(1) << shift;
    bigint x1 = (x0 * 2 + x / (x0 * x0)) / 3;
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
/// 計算 bigint 之整數立方根（icbrt 之別名）。
/// </summary>
/// <param name="x">輸入整數</param>
/// <returns>整數立方根</returns>
NUMERIC_NODISCARD inline bigint cbrt(const bigint& x) noexcept {
    return icbrt(x);
}

/// <summary>
/// 計算 bigint 之整數非負次方冪（快速冪演算法）。
/// </summary>
/// <param name="base">底數</param>
/// <param name="exp">無符號指數</param>
/// <returns>冪次結果</returns>
NUMERIC_NODISCARD inline bigint pow(const bigint& base, unsigned int exp) {
    if (exp == 0) {
        return bigint(1);
    }
    if (base == 0) {
        return bigint(0);
    }
    if (base == 1) {
        return bigint(1);
    }
    if (base == -1) {
        return (exp & 1u) ? bigint(-1) : bigint(1);
    }

    bigint res(1);
    bigint b = base;
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
/// 計算 bigint 之整數冪（支援 bigint 指數）。
/// </summary>
/// <param name="base">底數</param>
/// <param name="exp">指數</param>
/// <returns>冪次結果</returns>
/// <exception cref="std::invalid_argument">若指數為負且底數非 1 或 -1 時拋出</exception>
NUMERIC_NODISCARD inline bigint pow(const bigint& base, const bigint& exp) {
    if (exp.sign() < 0) {
        if (base == 1) {
            return bigint(1);
        }
        if (base == -1) {
            return (exp % 2 != 0) ? bigint(-1) : bigint(1);
        }
        NUMERIC_THROW_OR_ABORT(std::invalid_argument("pow: negative exponent in bigint pow"));
    }
    if (exp == 0) {
        return bigint(1);
    }
    if (base == 0) {
        return bigint(0);
    }
    if (base == 1) {
        return bigint(1);
    }
    if (base == -1) {
        return (exp % 2 != 0) ? bigint(-1) : bigint(1);
    }

    bigint res(1);
    bigint b = base;
    bigint e = exp;
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
/// 計算兩 bigint 之最大公因數 (Greatest Common Divisor)。
/// </summary>
/// <param name="a">第一個整數</param>
/// <param name="b">第二個整數</param>
/// <returns>最大公因數（恆為非負）</returns>
NUMERIC_NODISCARD inline bigint gcd(bigint a, bigint b) {
    a = (a.sign() < 0) ? -a : a;
    b = (b.sign() < 0) ? -b : b;
    while (b != 0) {
        bigint r = a % b;
        a = std::move(b);
        b = std::move(r);
    }
    return a;
}

/// <summary>
/// 計算兩 bigint 之最小公倍數 (Least Common Multiple)。
/// </summary>
/// <param name="a">第一個整數</param>
/// <param name="b">第二個整數</param>
/// <returns>最小公倍數（恆為非負）</returns>
NUMERIC_NODISCARD inline bigint lcm(const bigint& a, const bigint& b) {
    if (a == 0 || b == 0) {
        return bigint(0);
    }
    bigint g = gcd(a, b);
    bigint abs_a = (a.sign() < 0) ? -a : a;
    bigint abs_b = (b.sign() < 0) ? -b : b;
    return (abs_a / g) * abs_b;
}

} // namespace numeric

namespace std {

NUMERIC_CONSTEXPR_20 inline numeric::bigint abs(const numeric::bigint& x) noexcept {
    return numeric::abs(x);
}

inline numeric::bigint sqrt(const numeric::bigint& x) {
    return numeric::sqrt(x);
}

inline numeric::bigint isqrt(const numeric::bigint& x) {
    return numeric::isqrt(x);
}

inline numeric::bigint cbrt(const numeric::bigint& x) noexcept {
    return numeric::cbrt(x);
}

inline numeric::bigint icbrt(const numeric::bigint& x) noexcept {
    return numeric::icbrt(x);
}

inline numeric::bigint pow(const numeric::bigint& base, unsigned int exp) {
    return numeric::pow(base, exp);
}

inline numeric::bigint pow(const numeric::bigint& base, const numeric::bigint& exp) {
    return numeric::pow(base, exp);
}

inline numeric::bigint gcd(const numeric::bigint& a, const numeric::bigint& b) {
    return numeric::gcd(a, b);
}

inline numeric::bigint lcm(const numeric::bigint& a, const numeric::bigint& b) {
    return numeric::lcm(a, b);
}

} // namespace std

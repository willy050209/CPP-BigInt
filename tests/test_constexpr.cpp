// filepath: tests/test_constexpr.cpp
/**
 * @file test_constexpr.cpp
 * @brief Compile-time (`constexpr`) and runtime compatibility unit tests.
 *        編譯期 (`constexpr`) 與執行期相容性單元測試。
 */

#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <numeric/BigIntMath.hpp>
#include <array>
#include <cstdint>
#include <iostream>

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)

namespace test_ce_bigint {

// 1. Constant construction and proxy checks / 常數建構與常數代理
constexpr numeric::bigint b0;
static_assert(b0 == 0, "bigint default ctor must be 0");
static_assert(b0.is_zero(), "bigint b0 must be zero");
static_assert(b0.is_sbo(), "bigint b0 must be SBO");

constexpr numeric::bigint b_proxy_zero = numeric::bigint::zero;
constexpr numeric::bigint b_proxy_one = numeric::bigint::one;
static_assert(b_proxy_zero == 0, "bigint::zero proxy check");
static_assert(b_proxy_one == 1, "bigint::one proxy check");

static_assert(numeric::bigint::zero == 0, "bigint::zero == 0");
static_assert(numeric::bigint::one == 1, "bigint::one == 1");
static_assert(0 == numeric::bigint::zero, "0 == bigint::zero");
static_assert(1 == numeric::bigint::one, "1 == bigint::one");
static_assert(numeric::bigint::zero != 1, "bigint::zero != 1");
static_assert(numeric::bigint::one != 0, "bigint::one != 0");
static_assert(numeric::bigint::zero == numeric::bigint::zero, "zero == zero");
static_assert(numeric::bigint::zero != numeric::bigint::one, "zero != one");
static_assert(numeric::bigint::zero() == 0, "bigint::zero() == 0");
static_assert(numeric::bigint::one() == 1, "bigint::one() == 1");
static_assert(b_proxy_zero == numeric::bigint::zero, "b_proxy_zero == bigint::zero");
static_assert(b_proxy_one == numeric::bigint::one, "b_proxy_one == bigint::one");
static_assert(numeric::bigint::zero == b_proxy_zero, "bigint::zero == b_proxy_zero");
static_assert(numeric::bigint::one == b_proxy_one, "bigint::one == b_proxy_one");

constexpr numeric::BigInt512 b512_zero = numeric::BigInt512::zero;
constexpr numeric::BigInt512 b512_one = numeric::BigInt512::one;
static_assert(b512_zero == 0, "BigInt512::zero == 0");
static_assert(b512_one == 1, "BigInt512::one == 1");
static_assert(b512_zero == numeric::bigint::zero, "b512_zero == bigint::zero");
static_assert(numeric::BigInt512::zero == b512_zero, "BigInt512::zero == b512_zero");

constexpr numeric::bigint b42{42};
constexpr numeric::bigint b_neg{-100};
static_assert(b42 == 42, "bigint(42) == 42");
static_assert(b_neg == -100, "bigint(-100) == -100");
static_assert(b42.sign() == 1, "b42 sign must be 1");
static_assert(b_neg.sign() == -1, "b_neg sign must be -1");

// 2. Arithmetic / 算術運算
static_assert(b42 + 8 == 50, "42 + 8 == 50");
static_assert(b42 - 50 == -8, "42 - 50 == -8");
static_assert(b42 * 2 == 84, "42 * 2 == 84");
static_assert(b42 / 10 == 4, "42 / 10 == 4");
static_assert(b42 % 10 == 2, "42 % 10 == 2");

// 3. Bitwise and shifts / 位元運算與位移
static_assert((numeric::bigint{0b1100} & numeric::bigint{0b1010}) == 0b1000,
              "0b1100 & 0b1010 == 0b1000");
static_assert((numeric::bigint{0b1100} | numeric::bigint{0b1010}) == 0b1110,
              "0b1100 | 0b1010 == 0b1110");
static_assert((numeric::bigint{0b1100} ^ numeric::bigint{0b1010}) == 0b0110,
              "0b1100 ^ 0b1010 == 0b0110");
static_assert((numeric::bigint{1} << 10) == 1024, "1 << 10 == 1024");
static_assert((numeric::bigint{1024} >> 5) == 32, "1024 >> 5 == 32");

// 4. Comparisons and logic / 比較與邏輯運算
static_assert(b42 > 10, "42 > 10");
static_assert(b42 >= 42, "42 >= 42");
static_assert(b42 < 100, "42 < 100");
static_assert(b42 <= 42, "42 <= 42");
static_assert(b42 != 0, "42 != 0");
static_assert(static_cast<bool>(b42), "b42 is truthy");
static_assert(!numeric::bigint{0}, "bigint(0) is falsy");
static_assert((b42 && true) == true, "b42 && true");
static_assert((numeric::bigint{0} || false) == false, "0 || false");

// 5. Constexpr factorial evaluation / 常數表達式函式求值
[[nodiscard]] constexpr auto calc_factorial(std::int32_t n) -> numeric::bigint {
    numeric::bigint r{1};
    for (std::int32_t i = 2; i <= n; ++i) {
        r *= i;
    }
    return r;
}
static_assert(calc_factorial(10) == 3628800, "10! == 3628800");

// 6. String parsing / 字串解析
constexpr numeric::bigint b_from_str{"1234567890123456789"};
static_assert(b_from_str > 0, "parsed bigint > 0");
static_assert(b_from_str % 10 == 9, "parsed bigint % 10 == 9");

constexpr numeric::bigint b_multi{"123456789012345678901234567890"};
static_assert(b_multi > 0, "multi-chunk parsed bigint > 0");
static_assert(b_multi % 10 == 0, "multi-chunk parsed bigint % 10 == 0");

// 7. abs
static_assert(numeric::abs(numeric::bigint{-42}) == 42, "abs(bigint(-42)) == 42");

// 8. SBO ADC / SBB & in-place arithmetic / 就地運算
constexpr numeric::bigint c_a{"18446744073709551615"};
constexpr numeric::bigint c_b{"1"};
static_assert(c_a + c_b == numeric::bigint{"18446744073709551616"},
              "SBO carry addition in constexpr");
static_assert((c_a + c_b) - c_b == c_a, "SBO borrow subtraction in constexpr");

[[nodiscard]] constexpr auto test_in_place() -> numeric::bigint {
    numeric::bigint x{100};
    x += 50;
    x -= 30;
    return x;
}
static_assert(test_in_place() == 120, "in-place arithmetic in constexpr");

// 9. addmul_1_raw and submul_1_raw constexpr evaluation / 編譯期常數求值
[[nodiscard]] constexpr auto test_addmul_submul_ce() -> bool {
    std::array<std::uint64_t, 4> dst{100ULL, 200ULL, 300ULL, 400ULL};
    const std::array<std::uint64_t, 4> src{10ULL, 20ULL, 30ULL, 40ULL};
    const std::uint64_t multiplier{5ULL};

    const std::uint64_t c{
        numeric::detail::BigIntIntrinsics::addmul_1_raw(dst, src, multiplier)};
    if (c != 0ULL) {
        return false;
    }
    if (dst[0] != 150ULL || dst[1] != 300ULL || dst[2] != 450ULL || dst[3] != 600ULL) {
        return false;
    }

    const std::uint64_t b{
        numeric::detail::BigIntIntrinsics::submul_1_raw(dst, src, multiplier)};
    if (b != 0ULL) {
        return false;
    }
    if (dst[0] != 100ULL || dst[1] != 200ULL || dst[2] != 300ULL || dst[3] != 400ULL) {
        return false;
    }

    std::array<std::uint64_t, 2> dst2{0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
    const std::array<std::uint64_t, 2> src2{2ULL, 0ULL};
    const std::uint64_t c2{numeric::detail::BigIntIntrinsics::addmul_1_raw(dst2, src2, 2ULL)};
    if (c2 != 1ULL || dst2[0] != 3ULL || dst2[1] != 0ULL) {
        return false;
    }

    std::array<std::uint64_t, 2> dst3{3ULL, 0ULL};
    const std::array<std::uint64_t, 2> src3{2ULL, 0ULL};
    const std::uint64_t b3{numeric::detail::BigIntIntrinsics::submul_1_raw(dst3, src3, 2ULL)};
    if (b3 != 1ULL || dst3[0] != 0xFFFFFFFFFFFFFFFFULL || dst3[1] != 0xFFFFFFFFFFFFFFFFULL) {
        return false;
    }

    return true;
}
static_assert(test_addmul_submul_ce(), "addmul_1_raw and submul_1_raw constexpr test");

} // namespace test_ce_bigint

#endif // NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20

/// <summary>
/// Executes compile-time constant evaluation runtime compatibility tests.
/// 執行編譯期常數求值之執行期相容性驗證測試。
/// </summary>
auto run_test_constexpr() -> void {
    std::cout << "[Testing Constexpr Evaluation (C++20+)]\n";

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
    TEST_ASSERT(test_ce_bigint::calc_factorial(10) == 3628800);
    TEST_ASSERT(test_ce_bigint::b_from_str > 0);
    TEST_ASSERT(numeric::abs(numeric::bigint{-42}) == 42);
    TEST_ASSERT(test_ce_bigint::test_addmul_submul_ce());
    std::cout << "  -> Constexpr static_assert and runtime verification passed.\n";
#else
    std::cout << "  -> Skipped on C++ < 20.\n";
#endif
}

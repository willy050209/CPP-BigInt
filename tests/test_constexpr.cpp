#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <numeric/BigIntMath.hpp>
#include <iostream>

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)

// ----------------------------------------------------------------------------
// 編譯期驗證：BigInt
// ----------------------------------------------------------------------------
namespace test_ce_bigint {
    // 1. 常數建構與常數代理
    constexpr numeric::bigint b0;
    static_assert(b0 == 0, "bigint default ctor must be 0");
    static_assert(b0.is_zero(), "bigint b0 must be zero");
    static_assert(b0.is_sbo(), "bigint b0 must be SBO");

    constexpr numeric::bigint b_proxy_zero = numeric::bigint::zero;
    constexpr numeric::bigint b_proxy_one = numeric::bigint::one;
    static_assert(b_proxy_zero == 0, "bigint::zero proxy check");
    static_assert(b_proxy_one == 1, "bigint::one proxy check");

    // 直讀常數代理與純量及物件之編譯期比對 (避免建構完整 BasicBigInt 物件)
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

    // 跨 SBO 樣板參數常數初始化與靜態檢查
    constexpr numeric::BigInt512 b512_zero = numeric::BigInt512::zero;
    constexpr numeric::BigInt512 b512_one = numeric::BigInt512::one;
    static_assert(b512_zero == 0, "BigInt512::zero == 0");
    static_assert(b512_one == 1, "BigInt512::one == 1");
    static_assert(b512_zero == numeric::bigint::zero, "b512_zero == bigint::zero");
    static_assert(numeric::BigInt512::zero == b512_zero, "BigInt512::zero == b512_zero");

    constexpr numeric::bigint b42(42);
    constexpr numeric::bigint b_neg(-100);
    static_assert(b42 == 42, "bigint(42) == 42");
    static_assert(b_neg == -100, "bigint(-100) == -100");
    static_assert(b42.sign() == 1, "b42 sign must be 1");
    static_assert(b_neg.sign() == -1, "b_neg sign must be -1");

    // 2. 算術運算
    static_assert(b42 + 8 == 50, "42 + 8 == 50");
    static_assert(b42 - 50 == -8, "42 - 50 == -8");
    static_assert(b42 * 2 == 84, "42 * 2 == 84");
    static_assert(b42 / 10 == 4, "42 / 10 == 4");
    static_assert(b42 % 10 == 2, "42 % 10 == 2");

    // 3. 位元運算與位移
    static_assert((numeric::bigint(0b1100) & numeric::bigint(0b1010)) == 0b1000, "0b1100 & 0b1010 == 0b1000");
    static_assert((numeric::bigint(0b1100) | numeric::bigint(0b1010)) == 0b1110, "0b1100 | 0b1010 == 0b1110");
    static_assert((numeric::bigint(0b1100) ^ numeric::bigint(0b1010)) == 0b0110, "0b1100 ^ 0b1010 == 0b0110");
    static_assert((numeric::bigint(1) << 10) == 1024, "1 << 10 == 1024");
    static_assert((numeric::bigint(1024) >> 5) == 32, "1024 >> 5 == 32");

    // 4. 比較運算與邏輯運算
    static_assert(b42 > 10, "42 > 10");
    static_assert(b42 >= 42, "42 >= 42");
    static_assert(b42 < 100, "42 < 100");
    static_assert(b42 <= 42, "42 <= 42");
    static_assert(b42 != 0, "42 != 0");
    static_assert(static_cast<bool>(b42), "b42 is truthy");
    static_assert(!numeric::bigint(0), "bigint(0) is falsy");
    static_assert((b42 && true) == true, "b42 && true");
    static_assert((numeric::bigint(0) || false) == false, "0 || false");

    // 5. 常數表達式函式求值
    constexpr numeric::bigint calc_factorial(int n) {
        numeric::bigint r = 1;
        for (int i = 2; i <= n; ++i) {
            r *= i;
        }
        return r;
    }
    static_assert(calc_factorial(10) == 3628800, "10! == 3628800");

    // 6. 字串解析
    constexpr numeric::bigint b_from_str("1234567890123456789");
    static_assert(b_from_str > 0, "parsed bigint > 0");
    static_assert(b_from_str % 10 == 9, "parsed bigint % 10 == 9");

    // 多階 chunk 編譯期解析（> 19 位元）
    constexpr numeric::bigint b_multi("123456789012345678901234567890");
    static_assert(b_multi > 0, "multi-chunk parsed bigint > 0");
    static_assert(b_multi % 10 == 0, "multi-chunk parsed bigint % 10 == 0");

    // 7. abs
    static_assert(numeric::abs(numeric::bigint(-42)) == 42, "abs(bigint(-42)) == 42");

    // 8. SBO ADC / SBB 與 In-place 運算
    constexpr numeric::bigint c_a("18446744073709551615");
    constexpr numeric::bigint c_b("1");
    static_assert(c_a + c_b == numeric::bigint("18446744073709551616"), "SBO carry addition in constexpr");
    static_assert((c_a + c_b) - c_b == c_a, "SBO borrow subtraction in constexpr");

    constexpr numeric::bigint test_in_place() {
        numeric::bigint x(100);
        x += 50;
        x -= 30;
        return x;
    }
    static_assert(test_in_place() == 120, "in-place arithmetic in constexpr");
}

#endif // NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20

/// <summary>
/// 執行編譯期常數求值之執行期相容性驗證測試。
/// </summary>
void run_test_constexpr() {
    std::cout << "[Testing Constexpr Evaluation (C++20+)]" << std::endl;

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
    TEST_ASSERT(test_ce_bigint::calc_factorial(10) == 3628800);
    TEST_ASSERT(test_ce_bigint::b_from_str > 0);
    TEST_ASSERT(numeric::abs(numeric::bigint(-42)) == 42);
    std::cout << "  -> Constexpr static_assert and runtime verification passed." << std::endl;
#else
    std::cout << "  -> Skipped on C++ < 20 (standard does not support non-literal constexpr types)." << std::endl;
#endif
}

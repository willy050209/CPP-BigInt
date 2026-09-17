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
    constexpr numeric::bigint b_proxy_one = numeric::bigint::one();
    static_assert(b_proxy_zero == 0, "bigint::zero proxy check");
    static_assert(b_proxy_one == 1, "bigint::one proxy check");

    constexpr numeric::bigint b42(42);
    constexpr numeric::bigint b_neg(-100);
    static_assert(b42 == 42, "bigint(42) == 42");
    static_assert(b_neg == -100, "bigint(-100) == -100");
    static_assert(b42.sign() == 1, "b42 sign must be 1");
    static_assert(b_neg.sign() == -1, "b_neg sign must be -1");

    // 2. 算術運算
    constexpr numeric::bigint sum = b42 + 8;
    static_assert(sum == 50, "42 + 8 == 50");

    constexpr numeric::bigint diff = b42 - 50;
    static_assert(diff == -8, "42 - 50 == -8");

    constexpr numeric::bigint prod = b42 * 2;
    static_assert(prod == 84, "42 * 2 == 84");

    constexpr numeric::bigint quot = b42 / 10;
    static_assert(quot == 4, "42 / 10 == 4");

    constexpr numeric::bigint rem = b42 % 10;
    static_assert(rem == 2, "42 % 10 == 2");

    // 3. 位元運算與位移
    constexpr numeric::bigint b_and = numeric::bigint(0b1100) & numeric::bigint(0b1010);
    static_assert(b_and == 0b1000, "0b1100 & 0b1010 == 0b1000");

    constexpr numeric::bigint b_or = numeric::bigint(0b1100) | numeric::bigint(0b1010);
    static_assert(b_or == 0b1110, "0b1100 | 0b1010 == 0b1110");

    constexpr numeric::bigint b_xor = numeric::bigint(0b1100) ^ numeric::bigint(0b1010);
    static_assert(b_xor == 0b0110, "0b1100 ^ 0b1010 == 0b0110");

    constexpr numeric::bigint b_shl = numeric::bigint(1) << 10;
    static_assert(b_shl == 1024, "1 << 10 == 1024");

    constexpr numeric::bigint b_shr = numeric::bigint(1024) >> 5;
    static_assert(b_shr == 32, "1024 >> 5 == 32");

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
    constexpr numeric::bigint fact10 = calc_factorial(10);
    static_assert(fact10 == 3628800, "10! == 3628800");

    // 6. 字串解析
    constexpr numeric::bigint b_from_str("1234567890123456789");
    static_assert(b_from_str > 0, "parsed bigint > 0");
    static_assert(b_from_str % 10 == 9, "parsed bigint % 10 == 9");

    // 7. abs
    constexpr numeric::bigint bi_abs = numeric::abs(numeric::bigint(-42));
    static_assert(bi_abs == 42, "abs(bigint(-42)) == 42");
}

#endif // NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20

/// <summary>
/// 執行編譯期常數求值之執行期相容性驗證測試。
/// </summary>
void run_test_constexpr() {
    std::cout << "[Testing Constexpr Evaluation (C++20+)]" << std::endl;

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
    TEST_ASSERT(test_ce_bigint::fact10 == 3628800);
    TEST_ASSERT(test_ce_bigint::b_from_str > 0);
    TEST_ASSERT(test_ce_bigint::bi_abs == 42);
    std::cout << "  -> Constexpr static_assert and runtime verification passed." << std::endl;
#else
    std::cout << "  -> Skipped on C++ < 20 (standard does not support non-literal constexpr types)." << std::endl;
#endif
}

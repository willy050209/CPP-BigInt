#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <numeric/BigIntMath.hpp>
#include <sstream>
#include <string>
#include <cstdint>
#include <unordered_set>

#if NUMERIC_HAS_STD_FORMAT
#  include <format>
#endif

/// <summary>
/// 執行 numeric::bigint 完整單元測試套件。
/// </summary>
void run_test_bigint() {
    using numeric::bigint;

    std::cout << "--- Running BigInt Tests ---" << std::endl;

    // 1. 數字初始化測試（各整數型態 int8_t~int64_t、uint8_t~uint64_t、bool）
    {
        bigint b_bool_t(true);
        bigint b_bool_f(false);
        TEST_ASSERT(b_bool_t == 1);
        TEST_ASSERT(b_bool_f == 0);

        int8_t i8 = -42;
        int16_t i16 = -1234;
        int32_t i32 = -987654;
        int64_t i64 = -9223372036854775807LL;
        bigint bi8(i8), bi16(i16), bi32(i32), bi64(i64);
        TEST_ASSERT(bi8 == -42);
        TEST_ASSERT(bi16 == -1234);
        TEST_ASSERT(bi32 == -987654);
        TEST_ASSERT(bi64 == -9223372036854775807LL);

        uint8_t u8 = 255;
        uint16_t u16 = 65535;
        uint32_t u32 = 4294967295U;
        uint64_t u64 = 18446744073709551615ULL;
        bigint bu8(u8), bu16(u16), bu32(u32), bu64(u64);
        TEST_ASSERT(bu8 == 255);
        TEST_ASSERT(bu16 == 65535);
        TEST_ASSERT(bu32 == 4294967295ULL);
        TEST_ASSERT(bu64 == u64);

        bigint b_zero(0);
        TEST_ASSERT(b_zero == 0);
        TEST_ASSERT(b_zero.is_sbo());
    }

    // 2. SBO 驗證（<= 256 位元數值正常運作且零配置，大於 256 位元數值正確動態配置運作）
    {
        // 256-bit unsigned max = 2^256 - 1 (4 limbs, 剛好填滿 256-bit SBO)
        bigint sbo_max("115792089237316195423570985008687907853269984665640564039457584007913129639935");
        TEST_ASSERT(sbo_max.is_sbo());
        TEST_ASSERT(sbo_max.to_string() == "115792089237316195423570985008687907853269984665640564039457584007913129639935");

        // 負的 256-bit 數值也落在 4 limbs 以內
        bigint sbo_neg("-115792089237316195423570985008687907853269984665640564039457584007913129639935");
        TEST_ASSERT(sbo_neg.is_sbo());

        // 2^256 (5 limbs, 超出 256 位元需 heap 動態配置)
        bigint heap_num("115792089237316195423570985008687907853269984665640564039457584007913129639936");
        TEST_ASSERT(!heap_num.is_sbo());
        TEST_ASSERT(heap_num.to_string() == "115792089237316195423570985008687907853269984665640564039457584007913129639936");

        // 運算成長觸發 SBO -> Heap 轉移
        bigint grow = sbo_max;
        TEST_ASSERT(grow.is_sbo());
        grow += 1;
        TEST_ASSERT(!grow.is_sbo());
        TEST_ASSERT(grow == heap_num);

        // 運算衰減觸發 Heap -> SBO 回縮
        grow -= 1;
        TEST_ASSERT(grow.is_sbo());
        TEST_ASSERT(grow == sbo_max);
    }

    // 3. 字串建構、例外處理與非法字元
    {
        bigint b1("123456789012345678901234567890");
        TEST_ASSERT(b1.to_string() == "123456789012345678901234567890");

        bigint b2("-987654321098765432109876543210");
        TEST_ASSERT(b2.to_string() == "-987654321098765432109876543210");

        bigint b3("+42");
        TEST_ASSERT(b3 == 42);

        bigint b4("000000000000");
        TEST_ASSERT(b4 == 0);
        TEST_ASSERT(b4.to_string() == "0");

        bigint b5("-000");
        TEST_ASSERT(b5 == 0);
        TEST_ASSERT(b5.to_string() == "0");

        TEST_ASSERT_THROWS(bigint(""), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint("-"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint("+"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint("123a45"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint("  42"), std::invalid_argument);
    }

    // 4. 四則運算子（+, -, *, /, %）與混合運算
    {
        bigint a("100000000000000000000");
        bigint b("30000000000000000000");

        TEST_ASSERT((a + b).to_string() == "130000000000000000000");
        TEST_ASSERT((a - b).to_string() == "70000000000000000000");
        TEST_ASSERT((b - a).to_string() == "-70000000000000000000");
        TEST_ASSERT((a * b).to_string() == "3000000000000000000000000000000000000000");
        TEST_ASSERT((a / b).to_string() == "3");
        TEST_ASSERT((a % b).to_string() == "10000000000000000000");

        // 負數除法與取模 (truncating toward zero，與 C++ 標準除法語意一致)
        bigint n_pos(7), n_neg(-7), d_pos(3), d_neg(-3);
        TEST_ASSERT(n_pos / d_pos == 2);
        TEST_ASSERT(n_pos % d_pos == 1);

        TEST_ASSERT(n_neg / d_pos == -2);
        TEST_ASSERT(n_neg % d_pos == -1);

        TEST_ASSERT(n_pos / d_neg == -2);
        TEST_ASSERT(n_pos % d_neg == 1);

        TEST_ASSERT(n_neg / d_neg == 2);
        TEST_ASSERT(n_neg % d_neg == -1);

        // 除以零例外
        TEST_ASSERT_THROWS(a / bigint(0), std::invalid_argument);
        TEST_ASSERT_THROWS(a % bigint(0), std::invalid_argument);

        // 混合原生型別運算
        TEST_ASSERT(a + 42 == bigint("100000000000000000042"));
        TEST_ASSERT(42 + a == bigint("100000000000000000042"));
        TEST_ASSERT(a - 100 == bigint("99999999999999999900"));
        TEST_ASSERT(100 - a == bigint("-99999999999999999900"));
        TEST_ASSERT(a * 2 == bigint("200000000000000000000"));
        TEST_ASSERT(2 * a == bigint("200000000000000000000"));
        TEST_ASSERT(a / 10 == bigint("10000000000000000000"));
        TEST_ASSERT(a % 3 == 1);
    }

    // 5. 遞增/遞減運算子 (++ / --)
    {
        bigint x = 0;
        TEST_ASSERT(++x == 1);
        TEST_ASSERT(x++ == 1);
        TEST_ASSERT(x == 2);

        TEST_ASSERT(--x == 1);
        TEST_ASSERT(x-- == 1);
        TEST_ASSERT(x == 0);
        TEST_ASSERT(--x == -1);
    }

    // 6. 比較運算子 (==, !=, <, <=, >, >=)
    {
        bigint p10(10), p20(20), n10(-10), n20(-20), z(0);

        TEST_ASSERT(p10 < p20);
        TEST_ASSERT(p10 <= p20);
        TEST_ASSERT(p20 > p10);
        TEST_ASSERT(p20 >= p10);
        TEST_ASSERT(p10 == 10);
        TEST_ASSERT(p10 != p20);

        TEST_ASSERT(n20 < n10);
        TEST_ASSERT(n10 < z);
        TEST_ASSERT(z < p10);
        TEST_ASSERT(n20 < p10);

        // 跨型別比較
        TEST_ASSERT(p10 == 10L);
        TEST_ASSERT(10L == p10);
        TEST_ASSERT(p10 > 5);
        TEST_ASSERT(5 < p10);
        TEST_ASSERT(n10 < 0);
        TEST_ASSERT(0 > n10);
    }

    // 7. 位元運算子 (&, |, ^, ~, <<, >>) 與二補數語意
    {
        bigint a = 0b1100; // 12
        bigint b = 0b1010; // 10

        TEST_ASSERT((a & b) == 0b1000); // 8
        TEST_ASSERT((a | b) == 0b1110); // 14
        TEST_ASSERT((a ^ b) == 0b0110); // 6
        TEST_ASSERT(~bigint(0) == -1);
        TEST_ASSERT(~bigint(1) == -2);
        TEST_ASSERT(~bigint(-1) == 0);

        // 位移運算
        bigint one(1);
        TEST_ASSERT((one << 10) == 1024);
        TEST_ASSERT(((one << 10) >> 10) == 1);
        TEST_ASSERT((one << 128).limb_count() == 3); // 觸發 Heap
        TEST_ASSERT(((one << 128) >> 128) == 1);     // 回縮 SBO

        // 負數位移 (算術右移語意)
        bigint neg_four(-4);
        TEST_ASSERT((neg_four >> 1) == -2);
        TEST_ASSERT((neg_four >> 2) == -1);
        TEST_ASSERT((neg_four >> 5) == -1);
    }

    // 8. 條件邏輯運算子 (&&, ||, !) 與 C 語言「非零即真」語意
    {
        bigint b_zero(0);
        bigint b_one(1);
        bigint b_neg(-42);
        bigint b_large("12345678901234567890");

        TEST_ASSERT(!b_zero);
        TEST_ASSERT(!!b_one);
        TEST_ASSERT(!!b_neg);
        TEST_ASSERT(!!b_large);

        TEST_ASSERT(b_one && b_large);
        TEST_ASSERT(!(b_zero && b_one));
        TEST_ASSERT(b_zero || b_one);
        TEST_ASSERT(!(b_zero || bigint(0)));

        // 混合布林與整數短路模擬
        TEST_ASSERT(b_one && true);
        TEST_ASSERT(b_zero || true);
        TEST_ASSERT(b_one && 100);
        TEST_ASSERT(b_zero || 42);
    }

    // 9. 大數階乘（例如 100! 檢驗高精度連續乘法精確度）
    {
        bigint fact = 1;
        for (int i = 2; i <= 100; ++i) {
            fact *= i;
        }
        std::string expected_fact100 =
            "933262154439441526816992388562667004907159682643816214685929"
            "638952175999932299156089414639761565182862536979208272237582"
            "51185210916864000000000000000000000000";
        TEST_ASSERT(fact.to_string() == expected_fact100);
    }

    // 10. 串流輸出 (operator<<)
    {
        bigint val("9876543210987654321");
        std::ostringstream oss;
        oss << val;
        TEST_ASSERT(oss.str() == "9876543210987654321");
    }

    // 11. 常數代理（bigint::zero, bigint::one）
    {
        TEST_ASSERT(bigint::zero == 0);
        TEST_ASSERT(bigint::zero() == 0);
        TEST_ASSERT(bigint::one == 1);
        TEST_ASSERT(bigint::one() == 1);

        bigint v = 100;
        v = bigint::zero;
        TEST_ASSERT(v == 0);
        v = bigint::one();
        TEST_ASSERT(v == 1);
    }

    // 12. 數學函式庫測試 (BigIntMath)
    {
        TEST_ASSERT(numeric::abs(bigint(0)) == 0);
        TEST_ASSERT(numeric::abs(bigint(42)) == 42);
        TEST_ASSERT(numeric::abs(bigint(-42)) == 42);

        TEST_ASSERT(numeric::isqrt(bigint(0)) == 0);
        TEST_ASSERT(numeric::isqrt(bigint(144)) == 12);
        TEST_ASSERT(numeric::sqrt(bigint(144)) == 12);

        bigint b_sq("10000000000000000000000000000000000000000"); // 10^40
        bigint b_root("100000000000000000000"); // 10^20
        TEST_ASSERT(numeric::isqrt(b_sq) == b_root);

        TEST_ASSERT(numeric::icbrt(bigint(0)) == 0);
        TEST_ASSERT(numeric::icbrt(bigint(1)) == 1);
        TEST_ASSERT(numeric::icbrt(bigint(-1)) == -1);
        TEST_ASSERT(numeric::icbrt(bigint(27)) == 3);
        TEST_ASSERT(numeric::icbrt(bigint(-27)) == -3);
        TEST_ASSERT(numeric::cbrt(bigint(1000)) == 10);

        TEST_ASSERT(numeric::pow(bigint(2), 10u) == 1024);
        TEST_ASSERT(numeric::pow(bigint(10), 5u) == 100000);
        TEST_ASSERT(numeric::pow(bigint(2), bigint(10)) == 1024);

        TEST_ASSERT(numeric::gcd(bigint(48), bigint(18)) == 6);
        TEST_ASSERT(numeric::gcd(bigint(-48), bigint(18)) == 6);
        TEST_ASSERT(numeric::lcm(bigint(4), bigint(6)) == 12);
    }

    // 13. std::hash 與 std::unordered_set
    {
        std::unordered_set<numeric::bigint> bi_set;
        bi_set.insert(numeric::bigint(123456789));
        bi_set.insert(numeric::bigint(-42));
        bi_set.insert(numeric::bigint(0));
        bi_set.insert(numeric::bigint("123456789012345678901234567890"));

        TEST_ASSERT(bi_set.count(numeric::bigint(123456789)) == 1);
        TEST_ASSERT(bi_set.count(numeric::bigint(-42)) == 1);
        TEST_ASSERT(bi_set.count(numeric::bigint(0)) == 1);
        TEST_ASSERT(bi_set.count(numeric::bigint("123456789012345678901234567890")) == 1);
        TEST_ASSERT(bi_set.count(numeric::bigint(999)) == 0);

        size_t orig_size = bi_set.size();
        bi_set.insert(numeric::bigint(123456789));
        TEST_ASSERT(bi_set.size() == orig_size);
    }

#if NUMERIC_HAS_STD_FORMAT
    // 14. std::format
    {
        std::string s1 = std::format("val: {}", numeric::bigint(123456789));
        TEST_ASSERT(s1 == "val: 123456789");

        std::string s2 = std::format("negative: {}", numeric::bigint(-987654321));
        TEST_ASSERT(s2 == "negative: -987654321");
    }
#endif

    std::cout << "--- BigInt Tests Completed Successfully ---" << std::endl;
}

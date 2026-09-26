#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <string>
#include <cstdint>
#include <utility>

void run_test_operators() {
    using numeric::bigint;

    std::cout << "--- Running Complete Operators & In-Place Tests ---" << std::endl;

    // =========================================================================
    // 1. 單元運算子 (Unary Operators: +, -, ++, --, ~, !)
    // =========================================================================
    {
        bigint zero(0);
        bigint pos(42);
        bigint neg(-42);
        bigint large("1234567890123456789012345678901234567890");

        // 1.1 operator+
        TEST_ASSERT(+zero == 0);
        TEST_ASSERT(+pos == 42);
        TEST_ASSERT(+neg == -42);
        TEST_ASSERT(+large == large);

        // 1.2 operator-
        TEST_ASSERT(-zero == 0);
        TEST_ASSERT(-pos == -42);
        TEST_ASSERT(-neg == 42);
        TEST_ASSERT(-(-pos) == 42);
        TEST_ASSERT(-(-large) == large);

        // 1.3 前置與後置 ++ (Pre/Post Increment)
        bigint inc_val = -2;
        TEST_ASSERT(++inc_val == -1); // -2 -> -1
        TEST_ASSERT(++inc_val == 0);  // -1 -> 0
        TEST_ASSERT(++inc_val == 1);  // 0 -> 1
        TEST_ASSERT(inc_val++ == 1);  // returns 1, inc_val becomes 2
        TEST_ASSERT(inc_val == 2);

        // SBO 上限跨越遞增 (2^256 - 1 -> 2^256)
        bigint max256 = (bigint(1) << 256) - 1;
        TEST_ASSERT(max256.is_sbo());
        TEST_ASSERT(++max256 == (bigint(1) << 256));
        TEST_ASSERT(!max256.is_sbo());

        // 1.4 前置與後置 -- (Pre/Post Decrement)
        bigint dec_val = 2;
        TEST_ASSERT(--dec_val == 1);  // 2 -> 1
        TEST_ASSERT(--dec_val == 0);  // 1 -> 0
        TEST_ASSERT(--dec_val == -1); // 0 -> -1
        TEST_ASSERT(dec_val-- == -1); // returns -1, dec_val becomes -2
        TEST_ASSERT(dec_val == -2);

        // Heap 下限回縮遞減 (2^256 -> 2^256 - 1)
        TEST_ASSERT(--max256 == ((bigint(1) << 256) - 1));
        TEST_ASSERT(max256.is_sbo());

        // 1.5 operator~ (Bitwise NOT: ~a == -a - 1)
        TEST_ASSERT(~zero == -1);
        TEST_ASSERT(~bigint(1) == -2);
        TEST_ASSERT(~bigint(-1) == 0);
        TEST_ASSERT(~bigint(42) == -43);
        TEST_ASSERT(~bigint(-43) == 42);

        // 大數與 SBO 邊界之 ~a 恆等式
        bigint test_vals[] = {
            bigint(0),
            bigint(100),
            bigint(-100),
            bigint("18446744073709551615"), // 2^64 - 1
            bigint("-18446744073709551615"),
            (bigint(1) << 256) - 1,
            -((bigint(1) << 256) - 1),
            bigint(1) << 512,
            -(bigint(1) << 512)
        };
        for (const auto& v : test_vals) {
            TEST_ASSERT(~v == -v - 1);
            TEST_ASSERT(~(~v) == v);
        }

        // 1.6 operator! (Logical NOT)
        TEST_ASSERT(!zero == true);
        TEST_ASSERT(!pos == false);
        TEST_ASSERT(!neg == false);
        TEST_ASSERT(!large == false);
        TEST_ASSERT(!!pos == true);
        TEST_ASSERT(!!zero == false);
    }

    // =========================================================================
    // 2. 就地算術運算子 (In-Place Arithmetic: +=, -=, *=, /=, %=)
    // =========================================================================
    {
        // 2.1 operator+=
        bigint a = 100;
        a += 50;
        TEST_ASSERT(a == 150);
        a += -30;
        TEST_ASSERT(a == 120);
        a += -150;
        TEST_ASSERT(a == -30);
        a += -20;
        TEST_ASSERT(a == -50);
        a += 50;
        TEST_ASSERT(a == 0);
        a += 0;
        TEST_ASSERT(a == 0);

        // 原生整數混加
        bigint mixed = 10;
        mixed += static_cast<int8_t>(5);
        TEST_ASSERT(mixed == 15);
        mixed += static_cast<int64_t>(-20);
        TEST_ASSERT(mixed == -5);
        mixed += static_cast<uint64_t>(100);
        TEST_ASSERT(mixed == 95);

        // 2.2 operator-=
        bigint s = 200;
        s -= 50;
        TEST_ASSERT(s == 150);
        s -= 200;
        TEST_ASSERT(s == -50);
        s -= -100;
        TEST_ASSERT(s == 50);
        s -= 50;
        TEST_ASSERT(s == 0);
        s -= 0;
        TEST_ASSERT(s == 0);

        // 自我減法清零 (Self-aliasing: a -= a)
        bigint self_sub = 999999;
        self_sub -= self_sub;
        TEST_ASSERT(self_sub == 0);
        TEST_ASSERT(self_sub.is_zero());

        bigint self_sub_neg("-12345678901234567890");
        self_sub_neg -= self_sub_neg;
        TEST_ASSERT(self_sub_neg == 0);
        TEST_ASSERT(self_sub_neg.is_zero());

        // 原生整數混減
        bigint s_mixed = 100;
        s_mixed -= static_cast<int32_t>(30);
        TEST_ASSERT(s_mixed == 70);
        s_mixed -= static_cast<int64_t>(-10);
        TEST_ASSERT(s_mixed == 80);

        // 2.3 operator*=
        bigint m = 12;
        m *= 3;
        TEST_ASSERT(m == 36);
        m *= -2;
        TEST_ASSERT(m == -72);
        m *= -1;
        TEST_ASSERT(m == 72);
        m *= 1;
        TEST_ASSERT(m == 72);
        m *= 0;
        TEST_ASSERT(m == 0);

        // 自我乘法別名 (Self-aliasing: m *= m)
        bigint self_mul = 7;
        self_mul *= self_mul;
        TEST_ASSERT(self_mul == 49);
        self_mul *= self_mul;
        TEST_ASSERT(self_mul == 2401);

        bigint self_mul_neg = -5;
        self_mul_neg *= self_mul_neg;
        TEST_ASSERT(self_mul_neg == 25);

        // 跨越 SBO 邊界之乘法擴展
        bigint m_large = (bigint(1) << 128);
        TEST_ASSERT(m_large.is_sbo());
        m_large *= (bigint(1) << 130);
        TEST_ASSERT(m_large == (bigint(1) << 258));
        TEST_ASSERT(!m_large.is_sbo());

        // 2.4 operator/=
        bigint d = 100;
        d /= 2;
        TEST_ASSERT(d == 50);
        d /= -5;
        TEST_ASSERT(d == -10);
        d /= -2;
        TEST_ASSERT(d == 5);
        d /= 10; // 5 / 10 = 0
        TEST_ASSERT(d == 0);

        // 截斷向零規則 (Truncation toward zero)
        bigint d1(7), d2(-7), d3(7), d4(-7);
        d1 /= 3;  TEST_ASSERT(d1 == 2);
        d2 /= 3;  TEST_ASSERT(d2 == -2);
        d3 /= -3; TEST_ASSERT(d3 == -2);
        d4 /= -3; TEST_ASSERT(d4 == 2);

        // 自我除法 (Self-aliasing: a /= a)
        bigint self_div = 12345;
        self_div /= self_div;
        TEST_ASSERT(self_div == 1);

        bigint self_div_neg = -98765;
        self_div_neg /= self_div_neg;
        TEST_ASSERT(self_div_neg == 1);

        // 除以零例外
        bigint div_zero = 42;
        TEST_ASSERT_THROWS(div_zero /= 0, std::invalid_argument);
        TEST_ASSERT_THROWS(div_zero /= bigint(0), std::invalid_argument);

        // 2.5 operator%=
        bigint r1(7), r2(-7), r3(7), r4(-7);
        r1 %= 3;  TEST_ASSERT(r1 == 1);
        r2 %= 3;  TEST_ASSERT(r2 == -1);
        r3 %= -3; TEST_ASSERT(r3 == 1);
        r4 %= -3; TEST_ASSERT(r4 == -1);

        // 自我取模 (Self-aliasing: a %= a)
        bigint self_mod = 12345;
        self_mod %= self_mod;
        TEST_ASSERT(self_mod == 0);

        // |a| < |b|
        bigint r_small = 3;
        r_small %= 7;
        TEST_ASSERT(r_small == 3);

        // 模零例外
        bigint mod_zero = 42;
        TEST_ASSERT_THROWS(mod_zero %= 0, std::invalid_argument);
        TEST_ASSERT_THROWS(mod_zero %= bigint(0), std::invalid_argument);
    }

    // =========================================================================
    // 3. 就地位元與位移運算子 (In-Place Bitwise: &=, |=, ^=, <<=, >>=)
    // =========================================================================
    {
        // 3.1 operator&=
        bigint b_and = 0b1100;
        b_and &= 0b1010;
        TEST_ASSERT(b_and == 0b1000);
        b_and &= 0;
        TEST_ASSERT(b_and == 0);

        // 自我 &= (a &= a)
        bigint self_and = 12345;
        self_and &= self_and;
        TEST_ASSERT(self_and == 12345);

        // 負數二補數 &=
        bigint neg_and = -1; // all 1s in two's complement
        neg_and &= 0xFF;
        TEST_ASSERT(neg_and == 0xFF);

        // 3.2 operator|=
        bigint b_or = 0b1100;
        b_or |= 0b0011;
        TEST_ASSERT(b_or == 0b1111);
        b_or |= 0;
        TEST_ASSERT(b_or == 0b1111);

        // 自我 |= (a |= a)
        bigint self_or = 54321;
        self_or |= self_or;
        TEST_ASSERT(self_or == 54321);

        // 3.3 operator^=
        bigint b_xor = 0b1100;
        b_xor ^= 0b1010;
        TEST_ASSERT(b_xor == 0b0110);
        b_xor ^= 0;
        TEST_ASSERT(b_xor == 0b0110);

        // 自我 ^= 清零 (a ^= a == 0)
        bigint self_xor = 987654321;
        self_xor ^= self_xor;
        TEST_ASSERT(self_xor == 0);
        TEST_ASSERT(self_xor.is_zero());

        bigint self_xor_neg("-12345678901234567890");
        self_xor_neg ^= self_xor_neg;
        TEST_ASSERT(self_xor_neg == 0);
        TEST_ASSERT(self_xor_neg.is_zero());

        // 3.4 operator<<= (泛型位移型態)
        bigint shl_val = 1;
        shl_val <<= 0;
        TEST_ASSERT(shl_val == 1);
        shl_val <<= 1;
        TEST_ASSERT(shl_val == 2);
        shl_val <<= 10;
        TEST_ASSERT(shl_val == 2048);

        // 泛型型別測試 (uint8_t, int, size_t, uint64_t)
        shl_val = 1;
        shl_val <<= static_cast<uint8_t>(4);
        TEST_ASSERT(shl_val == 16);
        shl_val <<= static_cast<int32_t>(4);
        TEST_ASSERT(shl_val == 256);
        shl_val <<= static_cast<size_t>(8);
        TEST_ASSERT(shl_val == 65536);

        // SBO -> Heap 擴展
        bigint shl_sbo = 1;
        TEST_ASSERT(shl_sbo.is_sbo());
        shl_sbo <<= 300;
        TEST_ASSERT(!shl_sbo.is_sbo());
        TEST_ASSERT(shl_sbo == (bigint(1) << 300));

        // 3.5 operator>>= (泛型右移)
        bigint shr_val = 2048;
        shr_val >>= 0;
        TEST_ASSERT(shr_val == 2048);
        shr_val >>= 1;
        TEST_ASSERT(shr_val == 1024);
        shr_val >>= 10;
        TEST_ASSERT(shr_val == 1);
        shr_val >>= 5; // 超出長度，正數歸 0
        TEST_ASSERT(shr_val == 0);

        // Heap -> SBO 回縮
        bigint shr_heap = bigint(1) << 300;
        TEST_ASSERT(!shr_heap.is_sbo());
        shr_heap >>= 300;
        TEST_ASSERT(shr_heap == 1);
        TEST_ASSERT(shr_heap.is_sbo());

        // 負數算術右移保持符號位
        bigint neg_shift = -4;
        neg_shift >>= 1;
        TEST_ASSERT(neg_shift == -2);
        neg_shift >>= 1;
        TEST_ASSERT(neg_shift == -1);
        neg_shift >>= 100; // 負數向右移到底恆為 -1
        TEST_ASSERT(neg_shift == -1);
    }

    // =========================================================================
    // 4. 移動語意與 Direct-Result 運算子 (Move-Reuse 4-Overload Matrix)
    // =========================================================================
    {
        bigint a(100);
        bigint b(200);

        // 4.1 operator+ 重載矩陣
        bigint r1 = a + b;                               // (const&, const&)
        bigint r2 = bigint(100) + b;                     // (&&, const&)
        bigint r3 = a + bigint(200);                     // (const&, &&)
        bigint r4 = bigint(100) + bigint(200);           // (&&, &&)
        TEST_ASSERT(r1 == 300 && r2 == 300 && r3 == 300 && r4 == 300);

        // 4.2 operator- 重載矩陣
        bigint s1 = a - b;                               // (const&, const&)
        bigint s2 = bigint(100) - b;                     // (&&, const&)
        bigint s3 = bigint(100) - bigint(200);           // (&&, &&)
        TEST_ASSERT(s1 == -100 && s2 == -100 && s3 == -100);

        // 4.3 位元運算重載矩陣 (&, |, ^)
        bigint x(0b1100);
        bigint y(0b1010);
        TEST_ASSERT((bigint(0b1100) & y) == 0b1000);     // (&&, const&)
        TEST_ASSERT((x & bigint(0b1010)) == 0b1000);     // (const&, &&)
        TEST_ASSERT((bigint(0b1100) & bigint(0b1010)) == 0b1000); // (&&, &&)

        TEST_ASSERT((bigint(0b1100) | y) == 0b1110);
        TEST_ASSERT((x | bigint(0b1010)) == 0b1110);
        TEST_ASSERT((bigint(0b1100) | bigint(0b1010)) == 0b1110);

        TEST_ASSERT((bigint(0b1100) ^ y) == 0b0110);
        TEST_ASSERT((x ^ bigint(0b1010)) == 0b0110);
        TEST_ASSERT((bigint(0b1100) ^ bigint(0b1010)) == 0b0110);

        // 4.4 位移運算移動重載 (<<, >>)
        bigint m_shl = bigint(1) << 10;                  // (&&, shift)
        TEST_ASSERT(m_shl == 1024);
        bigint m_shr = bigint(1024) >> 10;               // (&&, shift)
        TEST_ASSERT(m_shr == 1);
    }

    // =========================================================================
    // 5. 跨型別雙向比較矩陣 (Comparison Operators Matrix)
    // =========================================================================
    {
        bigint val(42);

        // 5.1 反身性、對稱性與傳遞性
        TEST_ASSERT(val == val);
        TEST_ASSERT(val <= val);
        TEST_ASSERT(val >= val);
        TEST_ASSERT(!(val < val));
        TEST_ASSERT(!(val > val));
        TEST_ASSERT(!(val != val));

        bigint small_v(10), mid_v(20), large_v(30);
        TEST_ASSERT(small_v < mid_v && mid_v < large_v && small_v < large_v); // 傳遞性

        // 5.2 與所有 C++ 原生整數型別雙向比較 (LHS 與 RHS)
        // int8_t
        int8_t i8 = 42;
        TEST_ASSERT(val == i8);
        TEST_ASSERT(i8 == val);
        TEST_ASSERT(val >= i8 && i8 <= val);

        // int16_t
        int16_t i16_less = 40;
        TEST_ASSERT(val > i16_less);
        TEST_ASSERT(i16_less < val);

        // int32_t
        int32_t i32_greater = 100;
        TEST_ASSERT(val < i32_greater);
        TEST_ASSERT(i32_greater > val);

        // int64_t
        int64_t i64_neg = -1000;
        TEST_ASSERT(val > i64_neg);
        TEST_ASSERT(i64_neg < val);

        // uint8_t
        uint8_t u8 = 42;
        TEST_ASSERT(val == u8);
        TEST_ASSERT(u8 == val);

        // uint16_t
        uint16_t u16 = 50;
        TEST_ASSERT(val < u16);
        TEST_ASSERT(u16 > val);

        // uint32_t
        uint32_t u32 = 42;
        TEST_ASSERT(val == u32);
        TEST_ASSERT(u32 == val);

        // uint64_t
        uint64_t u64 = 18446744073709551615ULL;
        TEST_ASSERT(val < u64);
        TEST_ASSERT(u64 > val);
        bigint big_u64(u64);
        TEST_ASSERT(big_u64 == u64);
        TEST_ASSERT(u64 == big_u64);
    }

    // =========================================================================
    // 6. 條件邏輯運算子 (Logical Operators: &&, ||)
    // =========================================================================
    {
        bigint z(0);
        bigint p(1);
        bigint n(-1);

        // 真值表驗證
        TEST_ASSERT(!(z && z));
        TEST_ASSERT(!(z && p));
        TEST_ASSERT(!(p && z));
        TEST_ASSERT(p && p);
        TEST_ASSERT(p && n);
        TEST_ASSERT(n && n);

        TEST_ASSERT(!(z || z));
        TEST_ASSERT(z || p);
        TEST_ASSERT(p || z);
        TEST_ASSERT(p || p);
        TEST_ASSERT(n || z);

        // 與布林與原生整數混用
        TEST_ASSERT(p && true);
        TEST_ASSERT(!(z && true));
        TEST_ASSERT(z || false == false);
        TEST_ASSERT(z || true);
        TEST_ASSERT(p && 123);
        TEST_ASSERT(z || 456);
        TEST_ASSERT(123 && p);
        TEST_ASSERT(456 || z);
    }

    std::cout << "--- Complete Operators & In-Place Tests Passed Successfully ---" << std::endl;
}

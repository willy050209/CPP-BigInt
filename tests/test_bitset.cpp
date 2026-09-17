#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <numeric/Bitset.hpp>
#include <bitset>
#include <string>
#include <cstdint>
#include <iostream>
#include <stdexcept>

/// <summary>
/// 執行 numeric::bigint 與 std::bitset 雙向二補數轉換及二進位字串編解碼單元測試。
/// </summary>
void run_test_bitset() {
    using numeric::bigint;
    using numeric::to_bitset;
    using numeric::to_bigint;

    std::cout << "--- Running Bitset & Binary String Tests ---" << std::endl;

    // 1. 基本非負整數轉換測試（0, 1, 正數, 邊界數值）
    {
        bigint b0(0);
        auto bs0_8 = b0.to_bitset<8>();
        TEST_ASSERT(bs0_8 == std::bitset<8>("00000000"));
        TEST_ASSERT(bigint(bs0_8) == 0);

        bigint b1(1);
        auto bs1_8 = b1.to_bitset<8>();
        TEST_ASSERT(bs1_8 == std::bitset<8>("00000001"));
        TEST_ASSERT(bigint(bs1_8) == 1);

        bigint b42(42);
        auto bs42_8 = b42.to_bitset<8>();
        TEST_ASSERT(bs42_8 == std::bitset<8>("00101010"));
        TEST_ASSERT(bigint(bs42_8) == 42);

        bigint b255(255);
        auto bs255_8 = b255.to_bitset<8>();
        TEST_ASSERT(bs255_8 == std::bitset<8>("11111111"));
        TEST_ASSERT(bigint(bs255_8) == 255);

        // 0 位元 bitset 邊界測試
        auto bs0_0 = b0.to_bitset<0>();
        TEST_ASSERT(bs0_0 == std::bitset<0>());
        TEST_ASSERT(bigint(bs0_0) == 0);
        TEST_ASSERT(bigint(12345).to_bitset<0>() == std::bitset<0>());
    }

    // 2. 負數二補數 (Two's Complement) 轉換測試
    {
        // -1: 所有位元皆為 1
        bigint bm1(-1);
        TEST_ASSERT(bm1.to_bitset<8>() == std::bitset<8>("11111111"));
        TEST_ASSERT(bm1.to_bitset<16>() == std::bitset<16>("1111111111111111"));
        TEST_ASSERT(bm1.to_bitset<64>() == std::bitset<64>(~0ULL));

        // -5: 8 位元二補數為 11111011
        bigint bm5(-5);
        TEST_ASSERT(bm5.to_bitset<8>() == std::bitset<8>("11111011"));

        // -42: 16 位元二補數與原生 uint16_t 轉型一致
        bigint bm42(-42);
        TEST_ASSERT(bm42.to_bitset<16>() == std::bitset<16>(static_cast<uint16_t>(-42)));

        // 128 位元與 256 位元二補數全為 1
        auto bs_m1_128 = bm1.to_bitset<128>();
        TEST_ASSERT(bs_m1_128.all());
        auto bs_m1_256 = bm1.to_bitset<256>();
        TEST_ASSERT(bs_m1_256.all());

        // 二補數截斷與溢位迴繞語意
        // -256 的低 8 位元應為 00000000 (2^8 - 256 = 0)
        bigint bm256(-256);
        TEST_ASSERT(bm256.to_bitset<8>() == std::bitset<8>("00000000"));

        // -257 的低 8 位元應為 11111111 (2^8 - (257 % 256) = 255)
        bigint bm257(-257);
        TEST_ASSERT(bm257.to_bitset<8>() == std::bitset<8>("11111111"));

        // 大數負數二補數截斷測試: - (1 << 100)
        bigint neg_shift = -(bigint(1) << 100);
        auto bs_neg_128 = neg_shift.to_bitset<128>();
        for (size_t i = 0; i < 100; ++i) {
            TEST_ASSERT(!bs_neg_128.test(i));
        }
        for (size_t i = 100; i < 128; ++i) {
            TEST_ASSERT(bs_neg_128.test(i));
        }

        // bitset 轉回 bigint 時，按無符號解析為非負數值
        TEST_ASSERT(bigint(std::bitset<8>("11111111")) == 255);
        TEST_ASSERT(bigint(std::bitset<8>("11111011")) == 251);
        TEST_ASSERT(bigint(std::bitset<16>(static_cast<uint16_t>(-42))) == static_cast<uint16_t>(-42));
    }

    // 3. 超過 64 位元之大位寬 (128, 256, 70 bits) 雙向轉換測試
    {
        // 128 位元整數測試
        bigint b128 = (bigint(1) << 120) + (bigint(1) << 65) + 987654321ULL;
        auto bs128 = b128.to_bitset<128>();
        TEST_ASSERT(bs128.test(120));
        TEST_ASSERT(bs128.test(65));
        TEST_ASSERT(!bs128.test(64));
        TEST_ASSERT(!bs128.test(121));
        bigint roundtrip128(bs128);
        TEST_ASSERT(roundtrip128 == b128);

        // 256 位元整數測試
        bigint b256 = (bigint(1) << 250) + (bigint(1) << 130) + (bigint(1) << 70) + 12345ULL;
        auto bs256 = b256.to_bitset<256>();
        TEST_ASSERT(bs256.test(250));
        TEST_ASSERT(bs256.test(130));
        TEST_ASSERT(bs256.test(70));
        TEST_ASSERT(!bs256.test(69));
        TEST_ASSERT(!bs256.test(251));
        bigint roundtrip256(bs256);
        TEST_ASSERT(roundtrip256 == b256);

        // 非 64 倍數之特殊位元寬度 (70 bits)
        bigint b70 = (bigint(1) << 69) + 42;
        auto bs70 = b70.to_bitset<70>();
        TEST_ASSERT(bs70.test(69));
        TEST_ASSERT(!bs70.test(68));
        TEST_ASSERT(bs70.test(5));
        TEST_ASSERT(bs70.test(3));
        TEST_ASSERT(bs70.test(1));
        bigint roundtrip70(bs70);
        TEST_ASSERT(roundtrip70 == b70);
    }

    // 4. to_binary_string 測試
    {
        TEST_ASSERT(bigint(0).to_binary_string() == "0");
        TEST_ASSERT(bigint(1).to_binary_string() == "1");
        TEST_ASSERT(bigint(2).to_binary_string() == "10");
        TEST_ASSERT(bigint(5).to_binary_string() == "101");
        TEST_ASSERT(bigint(42).to_binary_string() == "101010");
        TEST_ASSERT(bigint(255).to_binary_string() == "11111111");

        TEST_ASSERT(bigint(-1).to_binary_string() == "-1");
        TEST_ASSERT(bigint(-2).to_binary_string() == "-10");
        TEST_ASSERT(bigint(-5).to_binary_string() == "-101");
        TEST_ASSERT(bigint(-42).to_binary_string() == "-101010");
        TEST_ASSERT(bigint(-255).to_binary_string() == "-11111111");

        // 跨 limb 大數之二進位字串輸出
        bigint large_b = (bigint(1) << 70) + 3;
        std::string s_large = large_b.to_binary_string();
        TEST_ASSERT(s_large.size() == 71);
        TEST_ASSERT(s_large.front() == '1');
        TEST_ASSERT(s_large.substr(s_large.size() - 2) == "11");
        for (size_t i = 1; i < s_large.size() - 2; ++i) {
            TEST_ASSERT(s_large[i] == '0');
        }

        std::string s_neg_large = (-large_b).to_binary_string();
        TEST_ASSERT(s_neg_large == "-" + s_large);
    }

    // 5. from_binary_string 測試（正負號、前綴、前導 0、異常處理）
    {
        TEST_ASSERT(bigint::from_binary_string("0") == 0);
        TEST_ASSERT(bigint::from_binary_string("+0") == 0);
        TEST_ASSERT(bigint::from_binary_string("-0") == 0);
        TEST_ASSERT(bigint::from_binary_string("0000") == 0);
        TEST_ASSERT(bigint::from_binary_string("+000") == 0);
        TEST_ASSERT(bigint::from_binary_string("-000") == 0);

        TEST_ASSERT(bigint::from_binary_string("1") == 1);
        TEST_ASSERT(bigint::from_binary_string("+1") == 1);
        TEST_ASSERT(bigint::from_binary_string("-1") == -1);

        TEST_ASSERT(bigint::from_binary_string("101") == 5);
        TEST_ASSERT(bigint::from_binary_string("+101") == 5);
        TEST_ASSERT(bigint::from_binary_string("-101") == -5);

        TEST_ASSERT(bigint::from_binary_string("101010") == 42);
        TEST_ASSERT(bigint::from_binary_string("-101010") == -42);

        // 支援 0b / 0B 前綴
        TEST_ASSERT(bigint::from_binary_string("0b101") == 5);
        TEST_ASSERT(bigint::from_binary_string("+0b101") == 5);
        TEST_ASSERT(bigint::from_binary_string("-0b101") == -5);
        TEST_ASSERT(bigint::from_binary_string("0B1010") == 10);
        TEST_ASSERT(bigint::from_binary_string("-0B1010") == -10);
        TEST_ASSERT(bigint::from_binary_string("0b0000") == 0);
        TEST_ASSERT(bigint::from_binary_string("-0b0000") == 0);

        // 支援多餘前導 0 解析
        TEST_ASSERT(bigint::from_binary_string("0000101") == 5);
        TEST_ASSERT(bigint::from_binary_string("-0000101") == -5);
        TEST_ASSERT(bigint::from_binary_string("0b0000101") == 5);
        TEST_ASSERT(bigint::from_binary_string("-0b0000101") == -5);

        // 大數雙向字串往返解析
        bigint large_val = (bigint(1) << 150) + (bigint(1) << 80) + 777;
        TEST_ASSERT(bigint::from_binary_string(large_val.to_binary_string()) == large_val);
        TEST_ASSERT(bigint::from_binary_string((-large_val).to_binary_string()) == -large_val);

        // 異常路徑與快速失敗 (Fail-Fast)
        TEST_ASSERT_THROWS(bigint::from_binary_string(""), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("+"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("-"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("0b"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("+0b"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("-0b"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("0B"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("10201"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("abc"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("101a01"), std::invalid_argument);
        TEST_ASSERT_THROWS(bigint::from_binary_string("-101 01"), std::invalid_argument);
    }

    // 6. numeric::Bitset.hpp 輔助函式測試 (to_bitset, to_bigint)
    {
        bigint b_pos(123456);
        auto bs_pos = to_bitset<32>(b_pos);
        TEST_ASSERT(bs_pos == std::bitset<32>(123456));
        TEST_ASSERT(to_bigint(bs_pos) == 123456);

        bigint b_neg(-5);
        auto bs_neg = to_bitset<8>(b_neg);
        TEST_ASSERT(bs_neg == std::bitset<8>("11111011"));
        TEST_ASSERT(to_bigint(bs_neg) == 251);

        auto bs_zero = to_bitset<0>(bigint(999));
        TEST_ASSERT(bs_zero == std::bitset<0>());
        TEST_ASSERT(to_bigint(bs_zero) == 0);
    }

    std::cout << "--- Bitset & Binary String Tests Completed Successfully ---" << std::endl;
}

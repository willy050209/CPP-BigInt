// filepath: tests/test_toom3.cpp
/**
 * @file test_toom3.cpp
 * @brief Toom-Cook 3-way multiplication unit tests.
 *        Toom-Cook 3-way 乘法單元測試。
 */

#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <ranges>
#include <span>

namespace {

using numeric::detail::BigIntCore;
using numeric::detail::BigIntStorage;

// Exact division of BigIntStorage by 3 / BigIntStorage 除以 3 的整除輔助函式
auto div_exact_3(BigIntStorage& a) -> void {
    if (a.m_size == 0U) {
        return;
    }
    std::uint64_t rem{0ULL};
    for (std::size_t i = a.m_size; i > 0U; --i) {
        std::uint64_t next_rem{0ULL};
        a.data()[i - 1U] = BigIntCore::div128_64(rem, a.data()[i - 1U], 3ULL, next_rem);
        rem = next_rem;
    }
    assert(rem == 0ULL && "Toom-3: division by 3 was not exact!");
    a.normalize();
}

// Slice BigIntStorage into chunk / 從 BigIntStorage 切割區塊
auto slice_limbs(const BigIntStorage& src,
                 std::size_t offset,
                 std::size_t len,
                 BigIntStorage& dst) -> void {
    if (offset >= src.m_size || len == 0U) {
        return;
    }
    const std::size_t avail{src.m_size - offset};
    const std::size_t actual_len{(avail < len) ? avail : len};
    dst.resize(actual_len, 0ULL);
    BigIntStorage::copy_limbs(dst.as_span(), src.as_span().subspan(offset, actual_len));
    dst.m_sign = 1;
    dst.normalize();
}

// Toom-3 reference test implementation / Toom-3 驗證測試實作
auto mul_toom3_test(BigIntStorage& res,
                    const BigIntStorage& a,
                    const BigIntStorage& b) -> void {
    const std::size_t n{(a.m_size > b.m_size) ? a.m_size : b.m_size};
    const std::size_t m{(n + 2U) / 3U};

    BigIntStorage a0;
    BigIntStorage a1;
    BigIntStorage a2;
    slice_limbs(a, 0U, m, a0);
    slice_limbs(a, m, m, a1);
    slice_limbs(a, 2U * m, a.m_size, a2);

    BigIntStorage b0;
    BigIntStorage b1;
    BigIntStorage b2;
    slice_limbs(b, 0U, m, b0);
    slice_limbs(b, m, m, b1);
    slice_limbs(b, 2U * m, b.m_size, b2);

    // Evaluation points: 0, 1, -1, 2, inf / 求值點
    BigIntStorage a0_a2;
    BigIntCore::add_signed(a0_a2, a0, a2);
    BigIntStorage p1;
    BigIntStorage p_m1;
    BigIntCore::add_signed(p1, a0_a2, a1);
    BigIntCore::sub_signed(p_m1, a0_a2, a1);

    BigIntStorage a2_shl1;
    BigIntStorage two_a2_plus_a1;
    BigIntStorage two_a2_plus_a1_shl1;
    BigIntStorage p2;
    BigIntCore::shift_left(a2_shl1, a2, 1U);
    BigIntCore::add_signed(two_a2_plus_a1, a2_shl1, a1);
    BigIntCore::shift_left(two_a2_plus_a1_shl1, two_a2_plus_a1, 1U);
    BigIntCore::add_signed(p2, two_a2_plus_a1_shl1, a0);

    BigIntStorage b0_b2;
    BigIntCore::add_signed(b0_b2, b0, b2);
    BigIntStorage q1;
    BigIntStorage q_m1;
    BigIntCore::add_signed(q1, b0_b2, b1);
    BigIntCore::sub_signed(q_m1, b0_b2, b1);

    BigIntStorage b2_shl1;
    BigIntStorage two_b2_plus_b1;
    BigIntStorage two_b2_plus_b1_shl1;
    BigIntStorage q2;
    BigIntCore::shift_left(b2_shl1, b2, 1U);
    BigIntCore::add_signed(two_b2_plus_b1, b2_shl1, b1);
    BigIntCore::shift_left(two_b2_plus_b1_shl1, two_b2_plus_b1, 1U);
    BigIntCore::add_signed(q2, two_b2_plus_b1_shl1, b0);

    // 5 pointwise multiplications / 五點乘法
    BigIntStorage v0;
    BigIntStorage v_inf;
    BigIntStorage v1;
    BigIntStorage v_m1;
    BigIntStorage v2;
    BigIntCore::mul_core(v0, a0, b0);
    BigIntCore::mul_core(v_inf, a2, b2);
    BigIntCore::mul_signed(v1, p1, q1);
    BigIntCore::mul_signed(v_m1, p_m1, q_m1);
    BigIntCore::mul_signed(v2, p2, q2);

    // Interpolation / 內插還原
    BigIntStorage sum_v1_vm1;
    BigIntStorage t1;
    BigIntCore::add_signed(sum_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t1, sum_v1_vm1, 1U);

    BigIntStorage diff_v1_vm1;
    BigIntStorage t2;
    BigIntCore::sub_signed(diff_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t2, diff_v1_vm1, 1U);

    BigIntStorage c2_step1;
    BigIntStorage c2;
    BigIntCore::sub_signed(c2_step1, t1, v0);
    BigIntCore::sub_signed(c2, c2_step1, v_inf);

    BigIntStorage v2_minus_v0;
    BigIntStorage v2_step;
    BigIntCore::sub_signed(v2_minus_v0, v2, v0);
    BigIntCore::shift_right(v2_step, v2_minus_v0, 1U);

    BigIntStorage v2_step2;
    BigIntCore::sub_signed(v2_step2, v2_step, t2);

    BigIntStorage eight_v_inf;
    BigIntStorage v2_step3;
    BigIntCore::shift_left(eight_v_inf, v_inf, 3U);
    BigIntCore::sub_signed(v2_step3, v2_step2, eight_v_inf);

    BigIntStorage two_c2;
    BigIntStorage three_c3;
    BigIntCore::shift_left(two_c2, c2, 1U);
    BigIntCore::sub_signed(three_c3, v2_step3, two_c2);

    BigIntStorage c3{three_c3};
    div_exact_3(c3);

    BigIntStorage c1;
    BigIntCore::sub_signed(c1, t2, c3);

    // Recombination / 重組
    BigIntStorage c1_shl;
    BigIntStorage c2_shl;
    BigIntStorage c3_shl;
    BigIntStorage c4_shl;
    BigIntCore::shift_left_limbs(c1_shl, c1, m);
    BigIntCore::shift_left_limbs(c2_shl, c2, 2U * m);
    BigIntCore::shift_left_limbs(c3_shl, c3, 3U * m);
    BigIntCore::shift_left_limbs(c4_shl, v_inf, 4U * m);

    BigIntStorage acc1;
    BigIntStorage acc2;
    BigIntStorage acc3;
    BigIntCore::add_signed(acc1, v0, c1_shl);
    BigIntCore::add_signed(acc2, acc1, c2_shl);
    BigIntCore::add_signed(acc3, acc2, c3_shl);
    BigIntCore::add_signed(res, acc3, c4_shl);
}

} // namespace

auto run_test_toom3() -> void {
    std::cout << "--- Running Toom-3 Multiplication Verification Tests ---\n";
    std::mt19937_64 rng{12345ULL};

    constexpr std::array<std::size_t, 10> test_sizes{
        32U, 48U, 64U, 96U, 128U, 160U, 200U, 256U, 384U, 512U};
    for (const std::size_t size : test_sizes) {
        for (std::int32_t trial = 0; trial < 10; ++trial) {
            BigIntStorage a;
            BigIntStorage b;
            a.resize(size);
            b.resize(size);
            for (std::size_t i = 0U; i < size; ++i) {
                a.data()[i] = rng();
                b.data()[i] = rng();
            }
            a.m_sign = 1;
            a.normalize();
            b.m_sign = 1;
            b.normalize();

            BigIntStorage expected;
            BigIntStorage actual;
            BigIntCore::mul_karatsuba(expected, a, b);
            mul_toom3_test(actual, a, b);

            const bool match{std::ranges::equal(expected.as_span(), actual.as_span())};
            if (!match) {
                std::cerr << "FAIL at size " << size << " trial " << trial << "!\n";
                std::cerr << "expected size: " << expected.m_size
                          << ", actual: " << actual.m_size << "\n";
            }
            TEST_ASSERT(match);
        }
    }
}

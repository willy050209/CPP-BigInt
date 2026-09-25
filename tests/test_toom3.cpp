#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include <cstring>

using namespace numeric::detail;

namespace {

// Helper: exact division of BigIntStorage by 3
static void div_exact_3(BigIntStorage& a) {
    if (a.m_size == 0) return;
    uint64_t rem = 0;
    for (size_t i = a.m_size; i > 0; --i) {
        uint64_t next_rem = 0;
        a.data()[i - 1] = BigIntCore::div128_64(rem, a.data()[i - 1], 3, next_rem);
        rem = next_rem;
    }
    assert(rem == 0 && "Toom-3: division by 3 was not exact!");
    a.normalize();
}

// Toom-3 implementation
void mul_toom3_test(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
    size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
    size_t m = (n + 2) / 3;

    // Slice A into a0, a1, a2
    BigIntStorage a0, a1, a2;
    if (a.m_size > 0) {
        size_t l0 = (a.m_size < m) ? a.m_size : m;
        a0.resize(l0, 0);
        BigIntStorage::copy_limbs(a0.data(), a.data(), l0);
        a0.m_sign = 1; a0.normalize();
    }
    if (a.m_size > m) {
        size_t l1 = (a.m_size < 2 * m) ? (a.m_size - m) : m;
        a1.resize(l1, 0);
        BigIntStorage::copy_limbs(a1.data(), a.data() + m, l1);
        a1.m_sign = 1; a1.normalize();
    }
    if (a.m_size > 2 * m) {
        size_t l2 = a.m_size - 2 * m;
        a2.resize(l2, 0);
        BigIntStorage::copy_limbs(a2.data(), a.data() + 2 * m, l2);
        a2.m_sign = 1; a2.normalize();
    }

    // Slice B into b0, b1, b2
    BigIntStorage b0, b1, b2;
    if (b.m_size > 0) {
        size_t l0 = (b.m_size < m) ? b.m_size : m;
        b0.resize(l0, 0);
        BigIntStorage::copy_limbs(b0.data(), b.data(), l0);
        b0.m_sign = 1; b0.normalize();
    }
    if (b.m_size > m) {
        size_t l1 = (b.m_size < 2 * m) ? (b.m_size - m) : m;
        b1.resize(l1, 0);
        BigIntStorage::copy_limbs(b1.data(), b.data() + m, l1);
        b1.m_sign = 1; b1.normalize();
    }
    if (b.m_size > 2 * m) {
        size_t l2 = b.m_size - 2 * m;
        b2.resize(l2, 0);
        BigIntStorage::copy_limbs(b2.data(), b.data() + 2 * m, l2);
        b2.m_sign = 1; b2.normalize();
    }

    // Evaluation points: 0, 1, -1, 2, inf
    // p0 = a0, q0 = b0
    // pinf = a2, qinf = b2
    // a0_plus_a2 = a0 + a2
    BigIntStorage a0_a2;
    BigIntCore::add_signed(a0_a2, a0, a2);
    BigIntStorage p1, p_m1;
    BigIntCore::add_signed(p1, a0_a2, a1);     // A(1) = a0 + a1 + a2
    BigIntCore::sub_signed(p_m1, a0_a2, a1);   // A(-1) = a0 - a1 + a2

    // p2 = 4*a2 + 2*a1 + a0 = 2*(2*a2 + a1) + a0
    BigIntStorage a2_shl1, two_a2_plus_a1, two_a2_plus_a1_shl1, p2;
    BigIntCore::shift_left(a2_shl1, a2, 1);
    BigIntCore::add_signed(two_a2_plus_a1, a2_shl1, a1);
    BigIntCore::shift_left(two_a2_plus_a1_shl1, two_a2_plus_a1, 1);
    BigIntCore::add_signed(p2, two_a2_plus_a1_shl1, a0);

    // B evaluations
    BigIntStorage b0_b2;
    BigIntCore::add_signed(b0_b2, b0, b2);
    BigIntStorage q1, q_m1;
    BigIntCore::add_signed(q1, b0_b2, b1);     // B(1) = b0 + b1 + b2
    BigIntCore::sub_signed(q_m1, b0_b2, b1);   // B(-1) = b0 - b1 + b2

    BigIntStorage b2_shl1, two_b2_plus_b1, two_b2_plus_b1_shl1, q2;
    BigIntCore::shift_left(b2_shl1, b2, 1);
    BigIntCore::add_signed(two_b2_plus_b1, b2_shl1, b1);
    BigIntCore::shift_left(two_b2_plus_b1_shl1, two_b2_plus_b1, 1);
    BigIntCore::add_signed(q2, two_b2_plus_b1_shl1, b0);

    // 5 Point Multiplications:
    // v0 = a0 * b0
    // v_inf = a2 * b2
    // v1 = p1 * q1
    // v_m1 = p_m1 * q_m1
    // v2 = p2 * q2
    BigIntStorage v0, v_inf, v1, v_m1, v2;
    BigIntCore::mul_core(v0, a0, b0);
    BigIntCore::mul_core(v_inf, a2, b2);
    BigIntCore::mul_signed(v1, p1, q1);
    BigIntCore::mul_signed(v_m1, p_m1, q_m1);
    BigIntCore::mul_signed(v2, p2, q2);

    // Interpolation:
    // c0 = v0
    // c4 = v_inf
    // t1 = (v1 + v_m1) / 2
    BigIntStorage sum_v1_vm1, t1;
    BigIntCore::add_signed(sum_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t1, sum_v1_vm1, 1);

    // t2 = (v1 - v_m1) / 2
    BigIntStorage diff_v1_vm1, t2;
    BigIntCore::sub_signed(diff_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t2, diff_v1_vm1, 1);

    // c2 = t1 - v0 - v_inf
    BigIntStorage c2_step1, c2;
    BigIntCore::sub_signed(c2_step1, t1, v0);
    BigIntCore::sub_signed(c2, c2_step1, v_inf);

    // v2_minus_v0 = v2 - v0
    BigIntStorage v2_minus_v0, v2_step;
    BigIntCore::sub_signed(v2_minus_v0, v2, v0);
    BigIntCore::shift_right(v2_step, v2_minus_v0, 1); // (v2 - v0)/2

    // (v2 - v0)/2 - t2
    BigIntStorage v2_step2;
    BigIntCore::sub_signed(v2_step2, v2_step, t2);

    // - 8 * v_inf
    BigIntStorage eight_v_inf, v2_step3;
    BigIntCore::shift_left(eight_v_inf, v_inf, 3);
    BigIntCore::sub_signed(v2_step3, v2_step2, eight_v_inf);

    // - 2 * c2
    BigIntStorage two_c2, three_c3;
    BigIntCore::shift_left(two_c2, c2, 1);
    BigIntCore::sub_signed(three_c3, v2_step3, two_c2);

    // c3 = three_c3 / 3
    BigIntStorage c3 = three_c3;
    div_exact_3(c3);

    // c1 = t2 - c3
    BigIntStorage c1;
    BigIntCore::sub_signed(c1, t2, c3);

    // Recombination:
    // C = c0 + (c1 << m) + (c2 << 2m) + (c3 << 3m) + (c4 << 4m)
    BigIntStorage c1_shl, c2_shl, c3_shl, c4_shl;
    BigIntCore::shift_left_limbs(c1_shl, c1, m);
    BigIntCore::shift_left_limbs(c2_shl, c2, 2 * m);
    BigIntCore::shift_left_limbs(c3_shl, c3, 3 * m);
    BigIntCore::shift_left_limbs(c4_shl, v_inf, 4 * m);

    BigIntStorage acc1, acc2, acc3;
    BigIntCore::add_signed(acc1, v0, c1_shl);
    BigIntCore::add_signed(acc2, acc1, c2_shl);
    BigIntCore::add_signed(acc3, acc2, c3_shl);
    BigIntCore::add_signed(res, acc3, c4_shl);
}

} // namespace

void run_test_toom3() {
    std::cout << "--- Running Toom-3 Multiplication Verification Tests ---" << std::endl;
    std::mt19937_64 rng(12345);

    std::vector<size_t> test_sizes = {32, 48, 64, 96, 128, 160, 200, 256, 384, 512};
    for (size_t size : test_sizes) {
        for (int trial = 0; trial < 10; ++trial) {
            BigIntStorage a, b;
            a.resize(size);
            b.resize(size);
            for (size_t i = 0; i < size; ++i) {
                a.data()[i] = rng();
                b.data()[i] = rng();
            }
            a.m_sign = 1; a.normalize();
            b.m_sign = 1; b.normalize();

            BigIntStorage expected, actual;
            BigIntCore::mul_karatsuba(expected, a, b);
            mul_toom3_test(actual, a, b);

            bool match = (expected.m_size == actual.m_size &&
                std::memcmp(expected.data(), actual.data(), expected.m_size * sizeof(uint64_t)) == 0);
            if (!match) {
                std::cerr << "FAIL at size " << size << " trial " << trial << "!\n";
                std::cerr << "expected size: " << expected.m_size << ", actual: " << actual.m_size << "\n";
            }
            TEST_ASSERT(match);
        }
    }
}

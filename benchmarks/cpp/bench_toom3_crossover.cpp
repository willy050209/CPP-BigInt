#include <numeric/BigInt.hpp>
#include "../include/bench_common.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

using namespace numeric::detail;

// Exact div 3 helper
static void div_exact_3(BigIntStorage& a) {
    if (a.m_size == 0) return;
    uint64_t rem = 0;
    for (size_t i = a.m_size; i > 0; --i) {
        uint64_t next_rem = 0;
        a.data()[i - 1] = BigIntCore::div128_64(rem, a.data()[i - 1], 3, next_rem);
        rem = next_rem;
    }
    a.normalize();
}

void mul_toom3_test(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
    size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
    size_t m = (n + 2) / 3;

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

    BigIntStorage a0_a2;
    BigIntCore::add_signed(a0_a2, a0, a2);
    BigIntStorage p1, p_m1;
    BigIntCore::add_signed(p1, a0_a2, a1);
    BigIntCore::sub_signed(p_m1, a0_a2, a1);

    BigIntStorage a2_shl1, two_a2_plus_a1, two_a2_plus_a1_shl1, p2;
    BigIntCore::shift_left(a2_shl1, a2, 1);
    BigIntCore::add_signed(two_a2_plus_a1, a2_shl1, a1);
    BigIntCore::shift_left(two_a2_plus_a1_shl1, two_a2_plus_a1, 1);
    BigIntCore::add_signed(p2, two_a2_plus_a1_shl1, a0);

    BigIntStorage b0_b2;
    BigIntCore::add_signed(b0_b2, b0, b2);
    BigIntStorage q1, q_m1;
    BigIntCore::add_signed(q1, b0_b2, b1);
    BigIntCore::sub_signed(q_m1, b0_b2, b1);

    BigIntStorage b2_shl1, two_b2_plus_b1, two_b2_plus_b1_shl1, q2;
    BigIntCore::shift_left(b2_shl1, b2, 1);
    BigIntCore::add_signed(two_b2_plus_b1, b2_shl1, b1);
    BigIntCore::shift_left(two_b2_plus_b1_shl1, two_b2_plus_b1, 1);
    BigIntCore::add_signed(q2, two_b2_plus_b1_shl1, b0);

    BigIntStorage v0, v_inf, v1, v_m1, v2;
    BigIntCore::mul_core(v0, a0, b0);
    BigIntCore::mul_core(v_inf, a2, b2);
    BigIntCore::mul_signed(v1, p1, q1);
    BigIntCore::mul_signed(v_m1, p_m1, q_m1);
    BigIntCore::mul_signed(v2, p2, q2);

    BigIntStorage sum_v1_vm1, t1;
    BigIntCore::add_signed(sum_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t1, sum_v1_vm1, 1);

    BigIntStorage diff_v1_vm1, t2;
    BigIntCore::sub_signed(diff_v1_vm1, v1, v_m1);
    BigIntCore::shift_right(t2, diff_v1_vm1, 1);

    BigIntStorage c2_step1, c2;
    BigIntCore::sub_signed(c2_step1, t1, v0);
    BigIntCore::sub_signed(c2, c2_step1, v_inf);

    BigIntStorage v2_minus_v0, v2_step;
    BigIntCore::sub_signed(v2_minus_v0, v2, v0);
    BigIntCore::shift_right(v2_step, v2_minus_v0, 1);

    BigIntStorage v2_step2;
    BigIntCore::sub_signed(v2_step2, v2_step, t2);

    BigIntStorage eight_v_inf, v2_step3;
    BigIntCore::shift_left(eight_v_inf, v_inf, 3);
    BigIntCore::sub_signed(v2_step3, v2_step2, eight_v_inf);

    BigIntStorage two_c2, three_c3;
    BigIntCore::shift_left(two_c2, c2, 1);
    BigIntCore::sub_signed(three_c3, v2_step3, two_c2);

    BigIntStorage c3 = three_c3;
    div_exact_3(c3);

    BigIntStorage c1;
    BigIntCore::sub_signed(c1, t2, c3);

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

int main() {
    std::mt19937_64 rng(42);
    std::cout << "=================================================================\n";
    std::cout << "         Karatsuba vs Toom-3 Multiplication Benchmark            \n";
    std::cout << "=================================================================\n";
    std::cout << std::left << std::setw(12) << "Limbs"
              << std::setw(12) << "Bits"
              << std::setw(18) << "Karatsuba (ns)"
              << std::setw(18) << "Toom-3 (ns)"
              << std::setw(12) << "Winner" << "\n";
    std::cout << "-----------------------------------------------------------------\n";

    std::vector<size_t> test_limbs = {48, 64, 80, 96, 128, 160, 200, 256, 384, 512, 768, 1024};
    for (size_t limbs : test_limbs) {
        size_t iters = 1000;
        if (limbs >= 128) iters = 200;
        if (limbs >= 256) iters = 50;
        if (limbs >= 512) iters = 15;
        if (limbs >= 1024) iters = 5;

        BigIntStorage a, b;
        a.resize(limbs);
        b.resize(limbs);
        for (size_t i = 0; i < limbs; ++i) {
            a.data()[i] = rng();
            b.data()[i] = rng();
        }
        a.m_sign = 1; a.normalize();
        b.m_sign = 1; b.normalize();

        BigIntStorage res_kara, res_toom;

        double time_kara = bench::measure_time_ns([&]() {
            BigIntCore::mul_karatsuba(res_kara, a, b);
            bench::do_not_optimize(res_kara);
        }, iters) / iters;

        double time_toom = bench::measure_time_ns([&]() {
            mul_toom3_test(res_toom, a, b);
            bench::do_not_optimize(res_toom);
        }, iters) / iters;

        std::string winner = (time_kara < time_toom) ? "Karatsuba" : "TOOM-3!";
        double ratio = (time_kara < time_toom) ? (time_toom / time_kara) : (time_kara / time_toom);

        std::cout << std::left << std::setw(12) << limbs
                  << std::setw(12) << (limbs * 64)
                  << std::setw(18) << std::fixed << std::setprecision(1) << time_kara
                  << std::setw(18) << std::fixed << std::setprecision(1) << time_toom
                  << winner << " (" << std::setprecision(2) << ratio << "x)\n";
    }

    return 0;
}

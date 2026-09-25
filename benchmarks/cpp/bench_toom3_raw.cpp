#include <numeric/BigInt.hpp>
#include "../include/bench_common.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

using namespace numeric::detail;

static void div_exact_3_raw(uint64_t* out, size_t len) {
    uint64_t rem = 0;
    for (size_t i = len; i > 0; --i) {
        uint64_t next_rem = 0;
        out[i - 1] = BigIntCore::div128_64(rem, out[i - 1], 3, next_rem);
        rem = next_rem;
    }
}

void mul_dispatch_test(uint64_t* NUMERIC_RESTRICT out,
                       const uint64_t* a, size_t a_len,
                       const uint64_t* b, size_t b_len,
                       uint64_t* scratch) noexcept;

void mul_toom3_raw(uint64_t* NUMERIC_RESTRICT out,
                   const uint64_t* a, size_t a_len,
                   const uint64_t* b, size_t b_len,
                   uint64_t* scratch) noexcept
{
    size_t n = (a_len > b_len) ? a_len : b_len;
    size_t m = (n + 2) / 3;

    const uint64_t* a0 = a;
    size_t a0_len = (a_len < m) ? a_len : m;
    while (a0_len > 0 && a0[a0_len - 1] == 0) --a0_len;

    const uint64_t* a1 = (a_len > m) ? (a + m) : nullptr;
    size_t a1_len = (a_len > m) ? ((a_len < 2 * m) ? (a_len - m) : m) : 0;
    while (a1_len > 0 && a1[a1_len - 1] == 0) --a1_len;

    const uint64_t* a2 = (a_len > 2 * m) ? (a + 2 * m) : nullptr;
    size_t a2_len = (a_len > 2 * m) ? (a_len - 2 * m) : 0;
    while (a2_len > 0 && a2[a2_len - 1] == 0) --a2_len;

    const uint64_t* b0 = b;
    size_t b0_len = (b_len < m) ? b_len : m;
    while (b0_len > 0 && b0[b0_len - 1] == 0) --b0_len;

    const uint64_t* b1 = (b_len > m) ? (b + m) : nullptr;
    size_t b1_len = (b_len > m) ? ((b_len < 2 * m) ? (b_len - m) : m) : 0;
    while (b1_len > 0 && b1[b1_len - 1] == 0) --b1_len;

    const uint64_t* b2 = (b_len > 2 * m) ? (b + 2 * m) : nullptr;
    size_t b2_len = (b_len > 2 * m) ? (b_len - 2 * m) : 0;
    while (b2_len > 0 && b2[b2_len - 1] == 0) --b2_len;

    size_t ev_sz = m + 2;
    size_t pr_sz = 2 * m + 4;

    uint64_t* p1 = scratch;
    uint64_t* q1 = p1 + ev_sz;
    uint64_t* p_m1 = q1 + ev_sz;
    uint64_t* q_m1 = p_m1 + ev_sz;
    uint64_t* p2 = q_m1 + ev_sz;
    uint64_t* q2 = p2 + ev_sz;

    uint64_t* v1 = q2 + ev_sz;
    uint64_t* v_m1 = v1 + pr_sz;
    uint64_t* v2 = v_m1 + pr_sz;
    uint64_t* next_scratch = v2 + pr_sz;

    BigIntStorage::zero_limbs(scratch, (v2 + pr_sz) - scratch);
    BigIntStorage::zero_limbs(out, a_len + b_len);

    if (a0_len > 0 && b0_len > 0) {
        mul_dispatch_test(out, a0, a0_len, b0, b0_len, next_scratch);
    }

    uint64_t* v_inf = out + 4 * m;
    if (a2_len > 0 && b2_len > 0) {
        mul_dispatch_test(v_inf, a2, a2_len, b2, b2_len, next_scratch);
    }

    BigIntStorage::copy_limbs(p1, a0, a0_len);
    if (a2_len > 0) BigIntCore::add_to_raw(p1, ev_sz, a2, a2_len);
    BigIntStorage::copy_limbs(p_m1, p1, ev_sz);
    if (a1_len > 0) BigIntCore::add_to_raw(p1, ev_sz, a1, a1_len);

    int8_t s_a = 1;
    size_t a0_a2_len = ev_sz;
    while (a0_a2_len > 0 && p_m1[a0_a2_len - 1] == 0) --a0_a2_len;
    int cmp_a = BigIntCore::compare_unsigned(p_m1, a0_a2_len, a1, a1_len);
    if (cmp_a >= 0) {
        if (a1_len > 0) BigIntCore::sub_from_raw(p_m1, ev_sz, a1, a1_len);
        s_a = 1;
    } else {
        uint64_t tmp[256];
        uint64_t* t_ptr = (ev_sz <= 256) ? tmp : next_scratch;
        BigIntStorage::copy_limbs(t_ptr, a1, a1_len);
        BigIntStorage::zero_limbs(t_ptr + a1_len, ev_sz - a1_len);
        BigIntCore::sub_from_raw(t_ptr, ev_sz, p_m1, a0_a2_len);
        BigIntStorage::copy_limbs(p_m1, t_ptr, ev_sz);
        s_a = -1;
    }

    if (a2_len > 0) {
        BigIntCore::add_to_raw(p2, ev_sz, a2, a2_len);
        BigIntCore::add_to_raw(p2, ev_sz, a2, a2_len);
    }
    if (a1_len > 0) BigIntCore::add_to_raw(p2, ev_sz, a1, a1_len);
    uint64_t carry = 0;
    for (size_t i = 0; i < ev_sz; ++i) {
        uint64_t cur = p2[i];
        p2[i] = (cur << 1) | carry;
        carry = cur >> 63;
    }
    if (a0_len > 0) BigIntCore::add_to_raw(p2, ev_sz, a0, a0_len);

    BigIntStorage::copy_limbs(q1, b0, b0_len);
    if (b2_len > 0) BigIntCore::add_to_raw(q1, ev_sz, b2, b2_len);
    BigIntStorage::copy_limbs(q_m1, q1, ev_sz);
    if (b1_len > 0) BigIntCore::add_to_raw(q1, ev_sz, b1, b1_len);

    int8_t s_b = 1;
    size_t b0_b2_len = ev_sz;
    while (b0_b2_len > 0 && q_m1[b0_b2_len - 1] == 0) --b0_b2_len;
    int cmp_b = BigIntCore::compare_unsigned(q_m1, b0_b2_len, b1, b1_len);
    if (cmp_b >= 0) {
        if (b1_len > 0) BigIntCore::sub_from_raw(q_m1, ev_sz, b1, b1_len);
        s_b = 1;
    } else {
        uint64_t tmp[256];
        uint64_t* t_ptr = (ev_sz <= 256) ? tmp : next_scratch;
        BigIntStorage::copy_limbs(t_ptr, b1, b1_len);
        BigIntStorage::zero_limbs(t_ptr + b1_len, ev_sz - b1_len);
        BigIntCore::sub_from_raw(t_ptr, ev_sz, q_m1, b0_b2_len);
        BigIntStorage::copy_limbs(q_m1, t_ptr, ev_sz);
        s_b = -1;
    }

    if (b2_len > 0) {
        BigIntCore::add_to_raw(q2, ev_sz, b2, b2_len);
        BigIntCore::add_to_raw(q2, ev_sz, b2, b2_len);
    }
    if (b1_len > 0) BigIntCore::add_to_raw(q2, ev_sz, b1, b1_len);
    carry = 0;
    for (size_t i = 0; i < ev_sz; ++i) {
        uint64_t cur = q2[i];
        q2[i] = (cur << 1) | carry;
        carry = cur >> 63;
    }
    if (b0_len > 0) BigIntCore::add_to_raw(q2, ev_sz, b0, b0_len);

    size_t p1_len = ev_sz; while (p1_len > 0 && p1[p1_len - 1] == 0) --p1_len;
    size_t q1_len = ev_sz; while (q1_len > 0 && q1[q1_len - 1] == 0) --q1_len;
    if (p1_len > 0 && q1_len > 0) {
        mul_dispatch_test(v1, p1, p1_len, q1, q1_len, next_scratch);
    }

    size_t pm1_len = ev_sz; while (pm1_len > 0 && p_m1[pm1_len - 1] == 0) --pm1_len;
    size_t qm1_len = ev_sz; while (qm1_len > 0 && q_m1[qm1_len - 1] == 0) --qm1_len;
    if (pm1_len > 0 && qm1_len > 0) {
        mul_dispatch_test(v_m1, p_m1, pm1_len, q_m1, qm1_len, next_scratch);
    }
    int8_t s_vm1 = s_a * s_b;

    size_t p2_len = ev_sz; while (p2_len > 0 && p2[p2_len - 1] == 0) --p2_len;
    size_t q2_len = ev_sz; while (q2_len > 0 && q2[q2_len - 1] == 0) --q2_len;
    if (p2_len > 0 && q2_len > 0) {
        mul_dispatch_test(v2, p2, p2_len, q2, q2_len, next_scratch);
    }

    BigIntStorage st_v0, st_vinf, st_v1, st_vm1, st_v2;
    st_v0.m_heap = out; st_v0.m_size = 2 * m; st_v0.m_capacity = 2 * m; st_v0.m_sign = 1; st_v0.normalize();
    st_vinf.m_heap = v_inf; st_vinf.m_size = (a_len + b_len > 4 * m) ? (a_len + b_len - 4 * m) : 0; st_vinf.m_capacity = st_vinf.m_size; st_vinf.m_sign = 1; st_vinf.normalize();
    st_v1.m_heap = v1; st_v1.m_size = pr_sz; st_v1.m_capacity = pr_sz; st_v1.m_sign = 1; st_v1.normalize();
    st_vm1.m_heap = v_m1; st_vm1.m_size = pr_sz; st_vm1.m_capacity = pr_sz; st_vm1.m_sign = s_vm1; st_vm1.normalize();
    st_v2.m_heap = v2; st_v2.m_size = pr_sz; st_v2.m_capacity = pr_sz; st_v2.m_sign = 1; st_v2.normalize();

    BigIntStorage sum_v1_vm1, t1;
    BigIntCore::add_signed(sum_v1_vm1, st_v1, st_vm1);
    BigIntCore::shift_right(t1, sum_v1_vm1, 1);

    BigIntStorage diff_v1_vm1, t2;
    BigIntCore::sub_signed(diff_v1_vm1, st_v1, st_vm1);
    BigIntCore::shift_right(t2, diff_v1_vm1, 1);

    BigIntStorage c2_step1, c2;
    BigIntCore::sub_signed(c2_step1, t1, st_v0);
    BigIntCore::sub_signed(c2, c2_step1, st_vinf);

    BigIntStorage v2_minus_v0, v2_step;
    BigIntCore::sub_signed(v2_minus_v0, st_v2, st_v0);
    BigIntCore::shift_right(v2_step, v2_minus_v0, 1);

    BigIntStorage v2_step2;
    BigIntCore::sub_signed(v2_step2, v2_step, t2);

    BigIntStorage eight_v_inf, v2_step3;
    BigIntCore::shift_left(eight_v_inf, st_vinf, 3);
    BigIntCore::sub_signed(v2_step3, v2_step2, eight_v_inf);

    BigIntStorage two_c2, three_c3;
    BigIntCore::shift_left(two_c2, c2, 1);
    BigIntCore::sub_signed(three_c3, v2_step3, two_c2);

    BigIntStorage c3 = three_c3;
    div_exact_3_raw(c3.data(), c3.m_size);
    c3.normalize();

    BigIntStorage c1;
    BigIntCore::sub_signed(c1, t2, c3);

    st_v0.m_heap = nullptr; st_vinf.m_heap = nullptr;
    st_v1.m_heap = nullptr; st_vm1.m_heap = nullptr; st_v2.m_heap = nullptr;

    if (c1.m_size > 0) BigIntCore::add_to_raw(out + m, (a_len + b_len) - m, c1.data(), c1.m_size);
    if (c2.m_size > 0) BigIntCore::add_to_raw(out + 2 * m, (a_len + b_len) - 2 * m, c2.data(), c2.m_size);
    if (c3.m_size > 0) BigIntCore::add_to_raw(out + 3 * m, (a_len + b_len) - 3 * m, c3.data(), c3.m_size);
}

void mul_dispatch_test(uint64_t* NUMERIC_RESTRICT out,
                       const uint64_t* a, size_t a_len,
                       const uint64_t* b, size_t b_len,
                       uint64_t* scratch) noexcept
{
    size_t n = (a_len > b_len) ? a_len : b_len;
    if (n >= 400) {
        mul_toom3_raw(out, a, a_len, b, b_len, scratch);
    } else {
        BigIntCore::mul_karatsuba_raw(out, a, a_len, b, b_len, scratch);
    }
}

int main() {
    std::mt19937_64 rng(42);
    std::cout << "Testing Recursive Toom-3 at 512, 768, 1024 limbs...\n";
    std::vector<size_t> test_limbs = {384, 512, 768, 1024};
    for (size_t limbs : test_limbs) {
        size_t iters = 15;
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
        res_kara.resize(2 * limbs, 0);
        res_toom.resize(2 * limbs, 0);

        std::vector<uint64_t> scratch_kara(64 * limbs + 1024);
        std::vector<uint64_t> scratch_toom(64 * limbs + 1024);

        double time_kara = bench::measure_time_ns([&]() {
            BigIntCore::mul_karatsuba_raw(res_kara.data(), a.data(), a.m_size, b.data(), b.m_size, scratch_kara.data());
            bench::do_not_optimize(res_kara.data());
        }, iters) / iters;

        double time_toom = bench::measure_time_ns([&]() {
            mul_dispatch_test(res_toom.data(), a.data(), a.m_size, b.data(), b.m_size, scratch_toom.data());
            bench::do_not_optimize(res_toom.data());
        }, iters) / iters;

        std::cout << "Limbs " << limbs << " (" << (limbs * 64) << " bits): "
                  << "Kara: " << time_kara << " ns, "
                  << "Toom-3: " << time_toom << " ns\n";
    }
    return 0;
}

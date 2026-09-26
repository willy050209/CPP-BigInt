#if __has_include(<gmpxx.h>)
#  include <gmpxx.h>
#  define HAVE_GMP 1
#  define USE_GMPXX 1
#elif __has_include(<gmp.h>)
#  include <gmp.h>
#  define HAVE_GMP 1
#  define USE_GMPXX 0
#else
#  define HAVE_GMP 0
#  define USE_GMPXX 0
#endif

#include "../include/bench_common.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

#if HAVE_GMP

#if !USE_GMPXX
// Lightweight RAII wrapper for raw mpz_t when gmpxx.h is not available
struct MpzWrapper {
    mpz_t val;
    MpzWrapper() { mpz_init(val); }
    explicit MpzWrapper(const std::string& s) { mpz_init_set_str(val, s.c_str(), 10); }
    ~MpzWrapper() { mpz_clear(val); }
    MpzWrapper(const MpzWrapper& o) { mpz_init_set(val, o.val); }
    MpzWrapper& operator=(const MpzWrapper& o) {
        if (this != &o) mpz_set(val, o.val);
        return *this;
    }
    MpzWrapper(MpzWrapper&& o) noexcept {
        val[0] = o.val[0];
        mpz_init(o.val);
    }
    MpzWrapper& operator=(MpzWrapper&& o) noexcept {
        if (this != &o) {
            mpz_swap(val, o.val);
        }
        return *this;
    }
};
using GmpInt = MpzWrapper;
#else
using GmpInt = mpz_class;
#endif

void run_benchmarks_for_tier(
    bench::BenchmarkReporter& reporter,
    const std::string& tier,
    int bits,
    const std::string& data_file,
    size_t arithmetic_iters,
    size_t mul_div_iters,
    size_t io_iters,
    size_t mem_pressure_iters)
{
    auto pairs = bench::load_dataset(data_file);
    if (pairs.empty()) return;

    size_t N = pairs.size();
    std::vector<GmpInt> a_nums;
    std::vector<GmpInt> b_nums;
    a_nums.reserve(N);
    b_nums.reserve(N);

    for (const auto& p : pairs) {
#if USE_GMPXX
        a_nums.emplace_back(mpz_class(p.a_dec));
        b_nums.emplace_back(mpz_class(p.b_dec));
#else
        a_nums.emplace_back(GmpInt(p.a_dec));
        b_nums.emplace_back(GmpInt(p.b_dec));
#endif
    }

    auto a_work = a_nums;
    auto setup = [&]() { a_work = a_nums; };

    // 1. Addition
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] + b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_add(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Add", arithmetic_iters * N, elapsed);
    }

    // 1b. Addition In-Place (Capacity Reuse)
    {
        double elapsed = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] += b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_add(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Add_InPlace", arithmetic_iters * N, elapsed);
    }

    // 2. Subtraction
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] - b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_sub(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Sub", arithmetic_iters * N, elapsed);
    }

    // 2b. Subtraction In-Place
    {
        double elapsed = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] -= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_sub(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Sub_InPlace", arithmetic_iters * N, elapsed);
    }

    // 3. Multiplication
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] * b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_mul(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mul", mul_div_iters * N, elapsed);
    }

    // 3b. Multiplication In-Place
    {
        double elapsed = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] *= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_mul(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mul_InPlace", mul_div_iters * N, elapsed);
    }

    // 4. Division
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] / b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_tdiv_q(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Div", mul_div_iters * N, elapsed);
    }

    // 4b. Division In-Place
    {
        double elapsed = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] /= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_tdiv_q(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Div_InPlace", mul_div_iters * N, elapsed);
    }

    // 5. Modulo
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] % b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_tdiv_r(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mod", mul_div_iters * N, elapsed);
    }

    // 5b. Modulo In-Place
    {
        double elapsed = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] %= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_tdiv_r(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mod_InPlace", mul_div_iters * N, elapsed);
    }

    // 5c. Bitwise AND (c = a & b) & In-Place (a &= b)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] & b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_and(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "And", arithmetic_iters * N, elapsed);

        double elapsed_ip = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] &= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_and(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "And_InPlace", arithmetic_iters * N, elapsed_ip);
    }

    // 5d. Bitwise OR (c = a | b) & In-Place (a |= b)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] | b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_ior(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Or", arithmetic_iters * N, elapsed);

        double elapsed_ip = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] |= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_ior(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Or_InPlace", arithmetic_iters * N, elapsed_ip);
    }

    // 5e. Bitwise XOR (c = a ^ b) & In-Place (a ^= b)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] ^ b_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_xor(c.val, a_nums[i].val, b_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Xor", arithmetic_iters * N, elapsed);

        double elapsed_ip = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] ^= b_nums[i];
                bench::do_not_optimize(a_work[i]);
#else
                mpz_xor(a_work[i].val, a_work[i].val, b_nums[i].val);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Xor_InPlace", arithmetic_iters * N, elapsed_ip);
    }

    // 5f. Shift Left (c = a << 17) & In-Place (a <<= 17)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] << 17;
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_mul_2exp(c.val, a_nums[i].val, 17);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Shl", arithmetic_iters * N, elapsed);

        double elapsed_ip = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] <<= 17;
                bench::do_not_optimize(a_work[i]);
#else
                mpz_mul_2exp(a_work[i].val, a_work[i].val, 17);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Shl_InPlace", arithmetic_iters * N, elapsed_ip);
    }

    // 5g. Shift Right (c = a >> 17) & In-Place (a >>= 17)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = a_nums[i] >> 17;
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_tdiv_q_2exp(c.val, a_nums[i].val, 17);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Shr", arithmetic_iters * N, elapsed);

        double elapsed_ip = bench::measure_inplace_time_ns(setup, [&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                a_work[i] >>= 17;
                bench::do_not_optimize(a_work[i]);
#else
                mpz_tdiv_q_2exp(a_work[i].val, a_work[i].val, 17);
                bench::do_not_optimize(a_work[i].val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Shr_InPlace", arithmetic_iters * N, elapsed_ip);
    }

    // 5h. Unary Negation (-a) & Bitwise NOT (~a)
    {
        double elapsed_neg = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = -a_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_neg(c.val, a_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Neg", arithmetic_iters * N, elapsed_neg);

        double elapsed_not = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class c = ~a_nums[i];
                bench::do_not_optimize(c);
#else
                GmpInt c;
                mpz_com(c.val, a_nums[i].val);
                bench::do_not_optimize(c.val);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Not", arithmetic_iters * N, elapsed_not);
    }

    // 5i. Comparison (a < b)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                bool c = (a_nums[i] < b_nums[i]);
                bench::do_not_optimize(c);
#else
                bool c = (mpz_cmp(a_nums[i].val, b_nums[i].val) < 0);
                bench::do_not_optimize(c);
#endif
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Cmp", arithmetic_iters * N, elapsed);
    }

    // 6. ToString(10)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                std::string s = a_nums[i].get_str(10);
                bench::do_not_optimize(s);
#else
                char* str = mpz_get_str(nullptr, 10, a_nums[i].val);
                bench::do_not_optimize(str);
                std::free(str);
#endif
            }
        }, io_iters);
        reporter.add_metric(tier, bits, "ToString_10", io_iters * N, elapsed);
    }

    // 7. FromString(10)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class val(pairs[i].a_dec);
                bench::do_not_optimize(val);
#else
                GmpInt val(pairs[i].a_dec);
                bench::do_not_optimize(val.val);
#endif
            }
        }, io_iters);
        reporter.add_metric(tier, bits, "FromString_10", io_iters * N, elapsed);
    }

    // 8. Memory Allocation Pressure
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class tmp = (a_nums[i] + b_nums[i]) - (a_nums[i] ^ b_nums[i]);
                bench::do_not_optimize(tmp);
#else
                GmpInt t1, t2, tmp;
                mpz_add(t1.val, a_nums[i].val, b_nums[i].val);
                mpz_xor(t2.val, a_nums[i].val, b_nums[i].val);
                mpz_sub(tmp.val, t1.val, t2.val);
                bench::do_not_optimize(tmp.val);
#endif
            }
        }, mem_pressure_iters);
        reporter.add_metric(tier, bits, "MemPressure", mem_pressure_iters * N, elapsed);
    }

    // 9. Chained temporary expression: (a + b) * (a - b)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
#if USE_GMPXX
                mpz_class d = (a_nums[i] + b_nums[i]) * (a_nums[i] - b_nums[i]);
                bench::do_not_optimize(d);
#else
                GmpInt t1, t2, d;
                mpz_add(t1.val, a_nums[i].val, b_nums[i].val);
                mpz_sub(t2.val, a_nums[i].val, b_nums[i].val);
                mpz_mul(d.val, t1.val, t2.val);
                bench::do_not_optimize(d.val);
#endif
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Chained_Expr", mul_div_iters * N, elapsed);
    }
}
#endif

int main(int argc, char** argv) {
#if !HAVE_GMP
    std::cerr << "Error: GMP headers/libraries not found (<gmpxx.h> or <gmp.h>).\n";
    return 1;
#else
    std::string data_dir = "benchmarks/data";
    std::string out_json = "benchmarks/results/results_cpp_gmp.json";

    if (argc > 1) data_dir = argv[1];
    if (argc > 2) out_json = argv[2];

    std::cout << "========================================================================\n";
    std::cout << "        Benchmark Target: C++ GMP / MPIR (x64)                         \n";
    std::cout << "========================================================================\n";

    bench::BenchmarkReporter reporter("C++ GMP");

    // Tier 1: Small
    run_benchmarks_for_tier(reporter, "small", 64,  data_dir + "/small_64.txt",  50, 50, 20, 50);
    run_benchmarks_for_tier(reporter, "small", 128, data_dir + "/small_128.txt", 50, 50, 20, 50);
    run_benchmarks_for_tier(reporter, "small", 256, data_dir + "/small_256.txt", 50, 50, 20, 50);

    // Tier 2: Medium
    run_benchmarks_for_tier(reporter, "medium", 512,  data_dir + "/medium_512.txt",  20, 20, 10, 20);
    run_benchmarks_for_tier(reporter, "medium", 1024, data_dir + "/medium_1024.txt", 20, 20, 10, 20);
    run_benchmarks_for_tier(reporter, "medium", 2048, data_dir + "/medium_2048.txt", 10, 10, 5,  10);
    run_benchmarks_for_tier(reporter, "medium", 4096, data_dir + "/medium_4096.txt", 5,  5,  2,  5);

    // Tier 3: Large
    run_benchmarks_for_tier(reporter, "large", 16384, data_dir + "/large_16384.txt", 3, 2, 1, 2);
    run_benchmarks_for_tier(reporter, "large", 32768, data_dir + "/large_32768.txt", 2, 1, 1, 1);
    run_benchmarks_for_tier(reporter, "large", 65536, data_dir + "/large_65536.txt", 1, 1, 1, 1);

    if (auto p = fs::path(out_json).parent_path(); !p.empty()) {
        fs::create_directories(p);
    }
    reporter.export_json(out_json);
    std::cout << "[GMP Benchmark] Results exported to " << out_json << std::endl;
    return 0;
#endif
}

#include "../include/bench_common.hpp"
#include "../include/bench_kernels.hpp"
#include <numeric/BigInt.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

void run_kernel_benchmarks(
    bench::BenchmarkReporter& reporter,
    const std::string& tier,
    int bits,
    const std::string& data_file,
    size_t iters_add,
    size_t iters_mul,
    size_t iters_div)
{
    auto pairs = bench::load_dataset(data_file);
    if (pairs.empty()) return;

    size_t N = pairs.size();
    size_t limbs = (bits + 63) / 64;

    // Convert dataset into raw contiguous limb buffers
    std::vector<std::vector<uint64_t>> a_limbs(N, std::vector<uint64_t>(limbs, 0));
    std::vector<std::vector<uint64_t>> b_limbs(N, std::vector<uint64_t>(limbs, 0));

    std::vector<size_t> a_lens(N), b_lens(N);
    for (size_t i = 0; i < N; ++i) {
        numeric::bigint a(pairs[i].a_dec);
        numeric::bigint b(pairs[i].b_dec);
        for (size_t l = 0; l < limbs && l < a.limb_count(); ++l) a_limbs[i][l] = a.limbs()[l];
        for (size_t l = 0; l < limbs && l < b.limb_count(); ++l) b_limbs[i][l] = b.limbs()[l];
        a_lens[i] = a.limb_count();
        b_lens[i] = (b.limb_count() > 0) ? b.limb_count() : 1;
    }

    // Preallocated buffers (0 heap allocation inside loops)
    std::vector<uint64_t> out_buf(2 * limbs + 32, 0);
    std::vector<uint64_t> scratch_buf(16 * limbs + 2048, 0);
    std::vector<uint64_t> q_buf(2 * limbs + 32, 0);
    std::vector<uint64_t> r_buf(2 * limbs + 32, 0);

    // =========================================================================
    // 1. ADD_N Kernel
    // =========================================================================
    {
        // CPP-BigInt Core (adc64 intrinsic)
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                uint64_t c = bench::kernels::cpp_bigint::raw_add_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(c);
            }
        }, iters_add);
        reporter.add_metric(tier, bits, "Kernel_Add_CPP", iters_add * N, elapsed, "layer1_kernel");
    }

#if BENCH_HAVE_GMP
    {
        // GMP mpn_add_n (Assembly)
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                uint64_t c = bench::kernels::gmp::raw_add_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(c);
            }
        }, iters_add);
        reporter.add_metric(tier, bits, "Kernel_Add_GMP", iters_add * N, elapsed, "layer1_kernel");
    }
#endif

    {
        // Scalar C++ loop baseline
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                uint64_t c = bench::kernels::scalar::raw_add_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(c);
            }
        }, iters_add);
        reporter.add_metric(tier, bits, "Kernel_Add_Scalar", iters_add * N, elapsed, "layer1_kernel");
    }

    // =========================================================================
    // 2. SUB_N Kernel
    // =========================================================================
    {
        // CPP-BigInt Core (sbb64 intrinsic)
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                uint64_t b = bench::kernels::cpp_bigint::raw_sub_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(b);
            }
        }, iters_add);
        reporter.add_metric(tier, bits, "Kernel_Sub_CPP", iters_add * N, elapsed, "layer1_kernel");
    }

#if BENCH_HAVE_GMP
    {
        // GMP mpn_sub_n (Assembly)
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                uint64_t b = bench::kernels::gmp::raw_sub_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(b);
            }
        }, iters_add);
        reporter.add_metric(tier, bits, "Kernel_Sub_GMP", iters_add * N, elapsed, "layer1_kernel");
    }
#endif

    // =========================================================================
    // 3. MULTIPLICATION Kernel
    // =========================================================================
    if (bits <= 2048) {
        // Schoolbook multiplication
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                std::fill_n(out_buf.data(), 2 * limbs, 0);
                bench::kernels::cpp_bigint::raw_mul_schoolbook(
                    out_buf.data(), a_limbs[i].data(), limbs, b_limbs[i].data(), limbs);
                bench::do_not_optimize(out_buf[0]);
            }
        }, iters_mul);
        reporter.add_metric(tier, bits, "Kernel_MulSchoolbook_CPP", iters_mul * N, elapsed, "layer1_kernel");
    }

    {
        // Full raw multiplication (Karatsuba or dispatch with preallocated scratch)
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                std::fill_n(out_buf.data(), 2 * limbs, 0);
                if (limbs >= 16) {
                    bench::kernels::cpp_bigint::raw_mul_karatsuba(
                        out_buf.data(), a_limbs[i].data(), limbs, b_limbs[i].data(), limbs, scratch_buf.data());
                } else {
                    bench::kernels::cpp_bigint::raw_mul_schoolbook(
                        out_buf.data(), a_limbs[i].data(), limbs, b_limbs[i].data(), limbs);
                }
                bench::do_not_optimize(out_buf[0]);
            }
        }, iters_mul);
        reporter.add_metric(tier, bits, "Kernel_Mul_CPP", iters_mul * N, elapsed, "layer1_kernel");
    }

#if BENCH_HAVE_GMP
    {
        // GMP mpn_mul_n
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                bench::kernels::gmp::raw_mul_n(
                    out_buf.data(), a_limbs[i].data(), b_limbs[i].data(), limbs);
                bench::do_not_optimize(out_buf[0]);
            }
        }, iters_mul);
        reporter.add_metric(tier, bits, "Kernel_Mul_GMP", iters_mul * N, elapsed, "layer1_kernel");
    }
#endif

    // =========================================================================
    // 4. DIVISION Kernel
    // =========================================================================
    if (iters_div > 0) {
        // CPP-BigInt Raw Division
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                bench::kernels::cpp_bigint::raw_div_bz(
                    q_buf.data(), r_buf.data(),
                    a_limbs[i].data(), a_lens[i],
                    b_limbs[i].data(), b_lens[i]);
                bench::do_not_optimize(q_buf[0]);
            }
        }, iters_div);
        reporter.add_metric(tier, bits, "Kernel_Div_CPP", iters_div * N, elapsed, "layer1_kernel");

#if BENCH_HAVE_GMP
        {
            // GMP mpn_tdiv_qr
            double elapsed_gmp = bench::measure_time_ns([&]() {
                for (size_t i = 0; i < N; ++i) {
                    bench::kernels::gmp::raw_div_qr(
                        q_buf.data(), r_buf.data(),
                        a_limbs[i].data(), a_lens[i],
                        b_limbs[i].data(), b_lens[i]);
                    bench::do_not_optimize(q_buf[0]);
                }
            }, iters_div);
            reporter.add_metric(tier, bits, "Kernel_Div_GMP", iters_div * N, elapsed_gmp, "layer1_kernel");
        }
#endif
    }
}

int main(int argc, char** argv) {
    std::string data_dir = "benchmarks/data";
    std::string out_json = "benchmarks/results/results_layer1_kernels.json";

    if (argc > 1) data_dir = argv[1];
    if (argc > 2) out_json = argv[2];

    std::cout << "========================================================================\n";
    std::cout << "     Layer 1 Benchmark: Pure Arithmetic Kernels (Zero Allocation)       \n";
    std::cout << "========================================================================\n";

    bench::BenchmarkReporter reporter("Layer1_Kernels");

    run_kernel_benchmarks(reporter, "small", 64,  data_dir + "/small_64.txt",  100, 100, 50);
    run_kernel_benchmarks(reporter, "small", 128, data_dir + "/small_128.txt", 100, 100, 50);
    run_kernel_benchmarks(reporter, "small", 256, data_dir + "/small_256.txt", 100, 100, 50);

    run_kernel_benchmarks(reporter, "medium", 512,  data_dir + "/medium_512.txt",  50, 50, 20);
    run_kernel_benchmarks(reporter, "medium", 1024, data_dir + "/medium_1024.txt", 50, 50, 20);
    run_kernel_benchmarks(reporter, "medium", 2048, data_dir + "/medium_2048.txt", 20, 20, 10);
    run_kernel_benchmarks(reporter, "medium", 4096, data_dir + "/medium_4096.txt", 10, 10, 5);

    run_kernel_benchmarks(reporter, "large", 16384, data_dir + "/large_16384.txt", 5, 3, 2);
    run_kernel_benchmarks(reporter, "large", 65536, data_dir + "/large_65536.txt", 2, 1, 1);

    if (auto p = fs::path(out_json).parent_path(); !p.empty()) {
        fs::create_directories(p);
    }
    reporter.export_json(out_json);
    std::cout << "[Layer 1 Benchmark] Results exported to " << out_json << std::endl;

    return 0;
}

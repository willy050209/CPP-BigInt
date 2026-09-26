#include "../include/bench_common.hpp"
#include "../include/bench_kernels.hpp"
#include <numeric/BigInt.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

void run_dense_sweep(bench::BenchmarkReporter& reporter, const std::string& data_dir) {
    std::cout << "\n--- Sub-bench 4.1: Dense Bit-Size Scaling Sweep (64 ~ 65536 bits) ---\n";

    std::vector<int> sweep_bits = {
        64, 128, 192, 256, 384, 512, 768, 1024, 1536,
        2048, 3072, 4096, 6144, 8192, 12288, 16384, 32768, 65536
    };

    for (int bits : sweep_bits) {
        std::string filename = data_dir + "/sweep_" + std::to_string(bits) + ".txt";
        auto pairs = bench::load_dataset(filename);
        if (pairs.empty()) continue;

        size_t N = pairs.size();
        std::vector<numeric::bigint> a_nums, b_nums;
        for (const auto& p : pairs) {
            a_nums.emplace_back(p.a_dec);
            b_nums.emplace_back(p.b_dec);
        }

        size_t iters_add = (bits <= 256) ? 50 : ((bits <= 4096) ? 20 : 2);
        size_t iters_mul = (bits <= 256) ? 50 : ((bits <= 4096) ? 10 : 1);
        size_t iters_div = (bits <= 256) ? 20 : ((bits <= 4096) ? 5 : 1);

        // Add
        double elapsed_add = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                numeric::bigint c = a_nums[i] + b_nums[i];
                bench::do_not_optimize(c);
            }
        }, iters_add);
        reporter.add_metric("sweep", bits, "Sweep_Add", iters_add * N, elapsed_add, "layer4_scaling");

        // Mul
        double elapsed_mul = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                numeric::bigint c = a_nums[i] * b_nums[i];
                bench::do_not_optimize(c);
            }
        }, iters_mul);
        reporter.add_metric("sweep", bits, "Sweep_Mul", iters_mul * N, elapsed_mul, "layer4_scaling");

        // Div
        double elapsed_div = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                numeric::bigint c = a_nums[i] / b_nums[i];
                bench::do_not_optimize(c);
            }
        }, iters_div);
        reporter.add_metric("sweep", bits, "Sweep_Div", iters_div * N, elapsed_div, "layer4_scaling");
    }
}

void run_mul_crossover_study(bench::BenchmarkReporter& reporter) {
    std::cout << "\n--- Sub-bench 4.2: Multiplication Crossover (Schoolbook vs Karatsuba) ---\n";

    // Test limb sizes from 4 to 32 (256 bits to 2048 bits)
    std::vector<size_t> limb_steps = { 4, 8, 12, 16, 20, 24, 28, 32 };
    size_t samples = 10;

    for (size_t limbs : limb_steps) {
        int bits = static_cast<int>(limbs * 64);
        std::vector<std::vector<uint64_t>> a_data(samples, std::vector<uint64_t>(limbs, 0xAAAAAAAAAAAAAAAAULL));
        std::vector<std::vector<uint64_t>> b_data(samples, std::vector<uint64_t>(limbs, 0x5555555555555555ULL));
        std::vector<uint64_t> out_buf(2 * limbs + 4, 0);
        std::vector<uint64_t> scratch(4 * limbs + 128, 0);

        size_t iters = 2000;

        // Force Schoolbook
        double elapsed_sb = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                std::fill_n(out_buf.data(), 2 * limbs, 0);
                bench::kernels::cpp_bigint::raw_mul_schoolbook(
                    out_buf.data(), a_data[i].data(), limbs, b_data[i].data(), limbs);
                bench::do_not_optimize(out_buf[0]);
            }
        }, iters);
        reporter.add_metric("crossover", bits, "Crossover_Mul_Schoolbook", iters * samples, elapsed_sb, "layer4_scaling");

        // Force Karatsuba
        double elapsed_kara = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                std::fill_n(out_buf.data(), 2 * limbs, 0);
                bench::kernels::cpp_bigint::raw_mul_karatsuba(
                    out_buf.data(), a_data[i].data(), limbs, b_data[i].data(), limbs, scratch.data());
                bench::do_not_optimize(out_buf[0]);
            }
        }, iters);
        reporter.add_metric("crossover", bits, "Crossover_Mul_Karatsuba", iters * samples, elapsed_kara, "layer4_scaling");
    }
}

void run_div_crossover_study(bench::BenchmarkReporter& reporter) {
    std::cout << "\n--- Sub-bench 4.3: Division Crossover (Knuth D vs Burnikel-Ziegler) ---\n";

    // Divisor limbs: 32, 64, 96, 128, 192, 256
    std::vector<size_t> v_limbs_list = { 32, 64, 96, 128, 192, 256 };
    size_t samples = 5;

    for (size_t v_len : v_limbs_list) {
        size_t u_len = 2 * v_len; // 2N / N standard division shape
        int bits = static_cast<int>(v_len * 64);

        std::vector<std::vector<uint64_t>> u_data(samples, std::vector<uint64_t>(u_len, 0xCCCCCCCCCCCCCCCCULL));
        std::vector<std::vector<uint64_t>> v_data(samples, std::vector<uint64_t>(v_len, 0x7777777777777777ULL));
        for (auto& v : v_data) v.back() |= 0x8000000000000000ULL; // Normalized

        std::vector<uint64_t> q_buf(u_len - v_len + 4, 0);
        std::vector<uint64_t> r_buf(v_len + 4, 0);
        std::vector<uint64_t> scratch(8 * u_len + 512, 0);

        size_t iters = 200;

        // Knuth Algorithm D
        double elapsed_knuth = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                bench::kernels::cpp_bigint::raw_div_knuth(
                    q_buf.data(), r_buf.data(),
                    u_data[i].data(), u_len,
                    v_data[i].data(), v_len);
                bench::do_not_optimize(q_buf[0]);
            }
        }, iters);
        reporter.add_metric("crossover", bits, "Crossover_Div_Knuth", iters * samples, elapsed_knuth, "layer4_scaling");

        // Burnikel-Ziegler Division
        double elapsed_bz = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                bench::kernels::cpp_bigint::raw_div_bz(
                    q_buf.data(), r_buf.data(),
                    u_data[i].data(), u_len,
                    v_data[i].data(), v_len);
                bench::do_not_optimize(q_buf[0]);
            }
        }, iters);
        reporter.add_metric("crossover", bits, "Crossover_Div_BZ", iters * samples, elapsed_bz, "layer4_scaling");
    }
}

int main(int argc, char** argv) {
    std::string data_dir = "benchmarks/data";
    std::string out_json = "benchmarks/results/results_layer4_scaling.json";

    if (argc > 1) data_dir = argv[1];
    if (argc > 2) out_json = argv[2];

    std::cout << "========================================================================\n";
    std::cout << "  Layer 4 Benchmark: Algorithm Crossover & Asymptotic Scaling Analysis \n";
    std::cout << "========================================================================\n";

    bench::BenchmarkReporter reporter("Layer4_Scaling");

    run_dense_sweep(reporter, data_dir);
    run_mul_crossover_study(reporter);
    run_div_crossover_study(reporter);

    if (auto p = fs::path(out_json).parent_path(); !p.empty()) {
        fs::create_directories(p);
    }
    reporter.export_json(out_json);
    std::cout << "[Layer 4 Benchmark] Results exported to " << out_json << std::endl;

    return 0;
}

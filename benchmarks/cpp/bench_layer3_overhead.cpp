#include "../include/bench_common.hpp"
#include <numeric/BigInt.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

void run_allocation_microbenchmarks(bench::BenchmarkReporter& reporter) {
    std::cout << "\n--- Sub-bench 3.1: Raw Allocator Overhead (malloc/free vs new[]/delete[]) ---\n";
    size_t iters = 200000;

    // Different byte sizes corresponding to BigInt storage
    size_t sizes[] = { 32, 64, 128, 256, 512, 1024, 2048, 8192 };
    for (size_t bytes : sizes) {
        int bits = static_cast<int>(bytes * 8);
        double ns_malloc = bench::measure_malloc_free_ns(bytes, iters);
        reporter.add_metric("overhead", bits, "Alloc_MallocFree", iters, ns_malloc, "layer3_overhead");

        size_t limbs = bytes / 8;
        double ns_new = bench::measure_new_delete_u64_ns(limbs, iters);
        reporter.add_metric("overhead", bits, "Alloc_NewDelete", iters, ns_new, "layer3_overhead");
    }
}

void run_sbo_cliff_benchmark(bench::BenchmarkReporter& reporter, const std::string& data_dir) {
    std::cout << "\n--- Sub-bench 3.2: SBO Boundary Cliff Analysis (192, 256, 320, 512 bits) ---\n";

    // 192, 256 bits are inside SBO (<= 4 limbs); 384, 512 bits are heap allocated (> 4 limbs)
    std::vector<int> cliff_bits = { 128, 192, 256, 384, 512 };
    for (int bits : cliff_bits) {
        std::string filename = data_dir + "/sweep_" + std::to_string(bits) + ".txt";
        auto pairs = bench::load_dataset(filename);
        if (pairs.empty()) continue;

        size_t N = pairs.size();
        std::vector<numeric::bigint> a_nums, b_nums;
        for (const auto& p : pairs) {
            a_nums.emplace_back(p.a_dec);
            b_nums.emplace_back(p.b_dec);
        }

        size_t iters = 10000;

        // Fresh addition (allocates heap if > 256 bits)
        double elapsed_fresh = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                numeric::bigint c = a_nums[i] + b_nums[i];
                bench::do_not_optimize(c);
            }
        }, iters);
        reporter.add_metric("cliff", bits, "SBO_Cliff_FreshAdd", iters * N, elapsed_fresh, "layer3_overhead");

        // In-place addition (reusing capacity)
        auto a_copy = a_nums;
        double elapsed_inplace = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                a_copy[i] += b_nums[i];
                bench::do_not_optimize(a_copy[i]);
            }
        }, iters);
        reporter.add_metric("cliff", bits, "SBO_Cliff_InPlaceAdd", iters * N, elapsed_inplace, "layer3_overhead");
    }
}

void run_inplace_vs_fresh_breakdown(bench::BenchmarkReporter& reporter, const std::string& data_dir) {
    std::cout << "\n--- Sub-bench 3.3: In-Place Mutation vs Fresh Object Differential ---\n";
    std::vector<int> test_bits = { 64, 128, 256, 512, 1024, 2048, 4096, 16384, 65536 };

    for (int bits : test_bits) {
        std::string prefix = (bits <= 256) ? "small" : ((bits <= 4096) ? "medium" : "large");
        std::string filename = data_dir + "/" + prefix + "_" + std::to_string(bits) + ".txt";
        auto pairs = bench::load_dataset(filename);
        if (pairs.empty()) continue;

        size_t N = pairs.size();
        std::vector<numeric::bigint> a_nums, b_nums;
        for (const auto& p : pairs) {
            a_nums.emplace_back(p.a_dec);
            b_nums.emplace_back(p.b_dec);
        }

        size_t iters = (bits <= 256) ? 50 : ((bits <= 4096) ? 20 : 2);

        // Fresh Addition
        double elapsed_fresh = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                numeric::bigint c = a_nums[i] + b_nums[i];
                bench::do_not_optimize(c);
            }
        }, iters);
        reporter.add_metric("breakdown", bits, "Diff_FreshAdd", iters * N, elapsed_fresh, "layer3_overhead");

        // In-Place Addition
        auto a_copy = a_nums;
        double elapsed_inplace = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                a_copy[i] += b_nums[i];
                bench::do_not_optimize(a_copy[i]);
            }
        }, iters);
        reporter.add_metric("breakdown", bits, "Diff_InPlaceAdd", iters * N, elapsed_inplace, "layer3_overhead");
    }
}

int main(int argc, char** argv) {
    std::string data_dir = "benchmarks/data";
    std::string out_json = "benchmarks/results/results_layer3_overhead.json";

    if (argc > 1) data_dir = argv[1];
    if (argc > 2) out_json = argv[2];

    std::cout << "========================================================================\n";
    std::cout << "  Layer 3 Benchmark: Allocation & Representation Cost Decomposition    \n";
    std::cout << "========================================================================\n";

    bench::BenchmarkReporter reporter("Layer3_Overhead");

    run_allocation_microbenchmarks(reporter);
    run_sbo_cliff_benchmark(reporter, data_dir);
    run_inplace_vs_fresh_breakdown(reporter, data_dir);

    if (auto p = fs::path(out_json).parent_path(); !p.empty()) {
        fs::create_directories(p);
    }
    reporter.export_json(out_json);
    std::cout << "[Layer 3 Benchmark] Results exported to " << out_json << std::endl;

    return 0;
}

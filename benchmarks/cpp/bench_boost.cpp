#if __has_include(<boost/multiprecision/cpp_int.hpp>)
#include <boost/multiprecision/cpp_int.hpp>
#define HAVE_BOOST 1
#else
#define HAVE_BOOST 0
#endif

#include "../include/bench_common.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

#if HAVE_BOOST
using boost_int = boost::multiprecision::cpp_int;

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
    std::vector<boost_int> a_nums;
    std::vector<boost_int> b_nums;
    a_nums.reserve(N);
    b_nums.reserve(N);

    for (const auto& p : pairs) {
        a_nums.emplace_back(boost_int(p.a_dec));
        b_nums.emplace_back(boost_int(p.b_dec));
    }

    // 1. Addition
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int c = a_nums[i] + b_nums[i];
                bench::do_not_optimize(c);
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Add", arithmetic_iters * N, elapsed);
    }

    // 2. Subtraction
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int c = a_nums[i] - b_nums[i];
                bench::do_not_optimize(c);
            }
        }, arithmetic_iters);
        reporter.add_metric(tier, bits, "Sub", arithmetic_iters * N, elapsed);
    }

    // 3. Multiplication
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int c = a_nums[i] * b_nums[i];
                bench::do_not_optimize(c);
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mul", mul_div_iters * N, elapsed);
    }

    // 4. Division
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int c = a_nums[i] / b_nums[i];
                bench::do_not_optimize(c);
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Div", mul_div_iters * N, elapsed);
    }

    // 5. Modulo
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int c = a_nums[i] % b_nums[i];
                bench::do_not_optimize(c);
            }
        }, mul_div_iters);
        reporter.add_metric(tier, bits, "Mod", mul_div_iters * N, elapsed);
    }

    // 6. ToString(10)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                std::string s = a_nums[i].str();
                bench::do_not_optimize(s);
            }
        }, io_iters);
        reporter.add_metric(tier, bits, "ToString_10", io_iters * N, elapsed);
    }

    // 7. FromString(10)
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int val(pairs[i].a_dec);
                bench::do_not_optimize(val);
            }
        }, io_iters);
        reporter.add_metric(tier, bits, "FromString_10", io_iters * N, elapsed);
    }

    // 8. Memory Allocation Pressure
    {
        double elapsed = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < N; ++i) {
                boost_int tmp = (a_nums[i] + b_nums[i]) - (a_nums[i] ^ b_nums[i]);
                bench::do_not_optimize(tmp);
            }
        }, mem_pressure_iters);
        reporter.add_metric(tier, bits, "MemPressure", mem_pressure_iters * N, elapsed);
    }
}
#endif

int main(int argc, char** argv) {
#if !HAVE_BOOST
    std::cerr << "Error: Boost.Multiprecision headers not found (<boost/multiprecision/cpp_int.hpp>).\n";
    return 1;
#else
    std::string data_dir = "benchmarks/data";
    std::string out_json = "benchmarks/results/results_cpp_boost.json";

    if (argc > 1) data_dir = argv[1];
    if (argc > 2) out_json = argv[2];

    std::cout << "========================================================================\n";
    std::cout << "  Benchmark Target: Boost.Multiprecision (cpp_int, MSVC x64)           \n";
    std::cout << "========================================================================\n";

    bench::BenchmarkReporter reporter("Boost.Multiprecision");

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
    std::cout << "[Boost Benchmark] Results exported to " << out_json << std::endl;
    return 0;
#endif
}

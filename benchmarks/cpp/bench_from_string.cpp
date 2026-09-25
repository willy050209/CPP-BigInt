#include <numeric/BigInt.hpp>
#include "../include/bench_common.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace numeric::detail;

template <size_t Threshold>
BigIntStorage parse_chunks_dc_thresh(const uint64_t* chunks, size_t start, size_t end) {
    size_t count = end - start;
    if (count <= Threshold) {
        return BigIntCore::parse_chunks_linear(chunks + start, count);
    }

    size_t k = 0;
    while ((static_cast<size_t>(1) << (k + 1)) < count) {
        ++k;
    }
    size_t split = static_cast<size_t>(1) << k;

    BigIntStorage low = parse_chunks_dc_thresh<Threshold>(chunks, start, start + split);
    BigIntStorage high = parse_chunks_dc_thresh<Threshold>(chunks, start + split, end);

    BigIntStorage dynamic_pow;
    const BigIntStorage* pow_ptr = nullptr;
    if (k < BigIntCore::Pow10Cache::MAX_LEVELS) {
        pow_ptr = &BigIntCore::Pow10Cache::instance().get_pow(k);
    } else {
        dynamic_pow = BigIntCore::compute_pow10_dynamic(k);
        pow_ptr = &dynamic_pow;
    }

    BigIntStorage high_scaled;
    BigIntCore::mul_core(high_scaled, high, *pow_ptr);

    BigIntStorage res;
    BigIntCore::add_signed(res, high_scaled, low);
    return res;
}

template <size_t Threshold>
void run_threshold_benchmark(const std::string& name, const std::vector<std::string>& strs, size_t iters) {
    // Extract chunks for each string
    std::vector<std::vector<uint64_t>> all_chunks;
    for (const auto& sv : strs) {
        std::vector<uint64_t> chunks;
        size_t curr_end = sv.size();
        while (curr_end > 0) {
            size_t chunk_start = (curr_end >= 19) ? (curr_end - 19) : 0;
            uint64_t val = 0;
            for (size_t i = chunk_start; i < curr_end; ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            chunks.push_back(val);
            curr_end = chunk_start;
        }
        all_chunks.push_back(chunks);
    }

    double elapsed = bench::measure_time_ns([&]() {
        for (const auto& ch : all_chunks) {
            BigIntStorage res = parse_chunks_dc_thresh<Threshold>(ch.data(), 0, ch.size());
            bench::do_not_optimize(res);
        }
    }, iters);

    double ns_per_op = elapsed / (strs.size() * iters);
    std::cout << "  Threshold " << std::setw(2) << Threshold << ": " << std::setw(8) << std::fixed << std::setprecision(1) << ns_per_op << " ns/op\n";
}

int main() {
    std::vector<std::pair<std::string, size_t>> datasets = {
        {"benchmarks/data/medium_512.txt", 20},
        {"benchmarks/data/medium_1024.txt", 20},
        {"benchmarks/data/medium_2048.txt", 10},
        {"benchmarks/data/medium_4096.txt", 5}
    };

    for (const auto& ds : datasets) {
        auto pairs = bench::load_dataset(ds.first);
        if (pairs.empty()) continue;
        std::vector<std::string> strs;
        for (const auto& p : pairs) strs.push_back(p.a_dec);

        std::cout << "\n=== Dataset: " << ds.first << " (" << strs[0].size() << " digits) ===\n";
        run_threshold_benchmark<6>(ds.first, strs, ds.second);
        run_threshold_benchmark<8>(ds.first, strs, ds.second);
        run_threshold_benchmark<10>(ds.first, strs, ds.second);
        run_threshold_benchmark<12>(ds.first, strs, ds.second);
        run_threshold_benchmark<16>(ds.first, strs, ds.second);
        run_threshold_benchmark<20>(ds.first, strs, ds.second);
        run_threshold_benchmark<24>(ds.first, strs, ds.second);
    }

    return 0;
}

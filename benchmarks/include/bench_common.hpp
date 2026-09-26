#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <functional>
#include <iomanip>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

namespace bench {

namespace detail {
#if defined(_MSC_VER)
#pragma optimize("", off)
    inline void use_char_ptr(char const volatile*) {}
#pragma optimize("", on)
#else
    inline void use_char_ptr(char const volatile*) {}
#endif
}

/// Prevents the compiler from optimizing away the result of a benchmarked expression.
template <typename T>
inline void do_not_optimize(const T& value) {
#if defined(_MSC_VER)
    detail::use_char_ptr(reinterpret_cast<char const volatile*>(&value));
#elif defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#else
    volatile const void* p = &value;
    (void)p;
#endif
}

inline void clobber_memory() {
#if defined(_MSC_VER)
    int dummy = 0;
    detail::use_char_ptr(reinterpret_cast<char const volatile*>(&dummy));
#elif defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : : "memory");
#endif
}

struct TestPair {
    std::string a_dec;
    std::string b_dec;
    std::string a_hex;
    std::string b_hex;
};

inline std::vector<TestPair> load_dataset(const std::string& filepath) {
    std::vector<TestPair> result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[bench_common] Error: Could not open dataset file: " << filepath << std::endl;
        return result;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        TestPair p;
        if (iss >> p.a_dec >> p.b_dec >> p.a_hex >> p.b_hex) {
            result.push_back(std::move(p));
        }
    }
    return result;
}

struct Metric {
    std::string target;
    std::string tier;
    int bits;
    std::string operation;
    size_t iterations;
    double total_ns;
    double ns_per_op;
    double ops_per_sec;
    std::string layer = "layer2_user";
};

class BenchmarkReporter {
private:
    std::string m_target_name;
    std::vector<Metric> m_metrics;

public:
    explicit BenchmarkReporter(std::string target_name)
        : m_target_name(std::move(target_name)) {}

    void add_metric(const std::string& tier, int bits, const std::string& op,
                    size_t iters, double total_ns, const std::string& layer = "layer2_user") {
        double ns_per_op = total_ns / static_cast<double>(iters);
        double ops_per_sec = (total_ns > 0.0) ? (static_cast<double>(iters) * 1e9 / total_ns) : 0.0;
        m_metrics.push_back(Metric{m_target_name, tier, bits, op, iters, total_ns, ns_per_op, ops_per_sec, layer});
        
        std::cout << std::left << std::setw(18) << m_target_name
                  << std::setw(14) << layer
                  << std::setw(8) << tier
                  << std::setw(6) << bits
                  << std::setw(18) << op
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << ns_per_op << " ns/op"
                  << std::setw(14) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/s"
                  << std::endl;
    }

    void export_json(const std::string& filepath) const {
        std::ofstream out(filepath);
        if (!out.is_open()) {
            std::cerr << "[bench_common] Error: Cannot write JSON to " << filepath << std::endl;
            return;
        }
        out << "{\n";
        out << "  \"target\": \"" << m_target_name << "\",\n";
        out << "  \"metrics\": [\n";
        for (size_t i = 0; i < m_metrics.size(); ++i) {
            const auto& m = m_metrics[i];
            out << "    {\n";
            out << "      \"layer\": \"" << m.layer << "\",\n";
            out << "      \"tier\": \"" << m.tier << "\",\n";
            out << "      \"bits\": " << m.bits << ",\n";
            out << "      \"operation\": \"" << m.operation << "\",\n";
            out << "      \"iterations\": " << m.iterations << ",\n";
            out << "      \"total_ns\": " << m.total_ns << ",\n";
            out << "      \"ns_per_op\": " << m.ns_per_op << ",\n";
            out << "      \"ops_per_sec\": " << m.ops_per_sec << "\n";
            out << "    }" << (i + 1 < m_metrics.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }
};

template <typename Func>
inline double measure_time_ns(Func&& f, size_t iterations, size_t warmup = 3, size_t samples = 7) {
    // Warmup
    for (size_t i = 0; i < warmup; ++i) {
        f();
    }
    clobber_memory();

    std::vector<double> sample_ns;
    sample_ns.reserve(samples);
    for (size_t s = 0; s < samples; ++s) {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            f();
        }
        clobber_memory();
        auto end = std::chrono::high_resolution_clock::now();
        sample_ns.push_back(static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
        ));
    }
    std::sort(sample_ns.begin(), sample_ns.end());
    return sample_ns[samples / 2]; // Return median sample
}

template <typename SetupFunc, typename OpFunc>
inline double measure_inplace_time_ns(SetupFunc&& setup, OpFunc&& op, size_t iterations, size_t warmup = 3, size_t samples = 7) {
    for (size_t i = 0; i < warmup; ++i) {
        setup();
        op();
    }
    clobber_memory();

    std::vector<double> sample_ns;
    sample_ns.reserve(samples);
    for (size_t s = 0; s < samples; ++s) {
        double total_ns = 0.0;
        for (size_t i = 0; i < iterations; ++i) {
            setup();
            clobber_memory();
            auto start = std::chrono::high_resolution_clock::now();
            op();
            auto end = std::chrono::high_resolution_clock::now();
            total_ns += static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
            );
        }
        sample_ns.push_back(total_ns);
    }
    std::sort(sample_ns.begin(), sample_ns.end());
    return sample_ns[samples / 2]; // Return median sample
}

inline double measure_malloc_free_ns(size_t bytes, size_t iters) {
    return measure_time_ns([bytes]() {
        void* p = std::malloc(bytes);
        bench::do_not_optimize(p);
        std::free(p);
    }, iters);
}

inline double measure_new_delete_u64_ns(size_t limbs, size_t iters) {
    return measure_time_ns([limbs]() {
        uint64_t* p = new uint64_t[limbs];
        bench::do_not_optimize(p);
        delete[] p;
    }, iters);
}

} // namespace bench

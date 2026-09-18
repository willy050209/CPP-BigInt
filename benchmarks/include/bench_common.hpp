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
    inline void use_char_ptr(char const volatile*) {}
}

/// Prevents the compiler from optimizing away the result of a benchmarked expression.
template <typename T>
inline void do_not_optimize(const T& value) {
#if defined(_MSC_VER)
    detail::use_char_ptr(reinterpret_cast<char const volatile*>(&value));
    _ReadWriteBarrier();
#elif defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#else
    volatile const void* p = &value;
    (void)p;
#endif
}

inline void clobber_memory() {
#if defined(_MSC_VER)
    _ReadWriteBarrier();
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
};

class BenchmarkReporter {
private:
    std::string m_target_name;
    std::vector<Metric> m_metrics;

public:
    explicit BenchmarkReporter(std::string target_name)
        : m_target_name(std::move(target_name)) {}

    void add_metric(const std::string& tier, int bits, const std::string& op,
                    size_t iters, double total_ns) {
        double ns_per_op = total_ns / static_cast<double>(iters);
        double ops_per_sec = (total_ns > 0.0) ? (static_cast<double>(iters) * 1e9 / total_ns) : 0.0;
        m_metrics.push_back(Metric{m_target_name, tier, bits, op, iters, total_ns, ns_per_op, ops_per_sec});
        
        std::cout << std::left << std::setw(16) << m_target_name
                  << std::setw(8) << tier
                  << std::setw(6) << bits
                  << std::setw(16) << op
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
inline double measure_time_ns(Func&& f, size_t iterations, size_t warmup = 5) {
    // Warmup
    for (size_t i = 0; i < warmup; ++i) {
        f();
    }
    clobber_memory();

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        f();
    }
    clobber_memory();
    auto end = std::chrono::high_resolution_clock::now();

    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
    );
}

} // namespace bench

// filepath: tests/test_helpers.hpp
#pragma once
/**
 * @file test_helpers.hpp
 * @brief Lightweight unit-test assertion helpers and statistics tracker.
 *        輕量級單元測試斷言輔助工具與統計追蹤器。
 */

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace numeric {
namespace test {

struct TestStats {
    std::int32_t total{0};
    std::int32_t passed{0};
    std::int32_t failed{0};
};

[[nodiscard]] inline auto GetGlobalTestStats() noexcept -> TestStats& {
    static TestStats stats;
    return stats;
}

} // namespace test
} // namespace numeric

#define TEST_ASSERT(cond)                                                          \
    do {                                                                           \
        ++numeric::test::GetGlobalTestStats().total;                               \
        if (cond) {                                                                \
            ++numeric::test::GetGlobalTestStats().passed;                          \
        } else {                                                                   \
            ++numeric::test::GetGlobalTestStats().failed;                          \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__                  \
                      << " - Assertion failed: " << #cond << "\n";                 \
        }                                                                          \
    } while (0)

#define TEST_ASSERT_THROWS(expr, exception_type)                                   \
    do {                                                                           \
        ++numeric::test::GetGlobalTestStats().total;                               \
        bool caught = false;                                                       \
        try {                                                                      \
            (void)(expr);                                                          \
        } catch (const exception_type&) {                                          \
            caught = true;                                                         \
        } catch (...) {                                                            \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__                  \
                      << " - Wrong exception type thrown for: " << #expr << "\n";  \
        }                                                                          \
        if (caught) {                                                              \
            ++numeric::test::GetGlobalTestStats().passed;                          \
        } else {                                                                   \
            ++numeric::test::GetGlobalTestStats().failed;                          \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__                  \
                      << " - Expected exception " << #exception_type               \
                      << " not thrown by: " << #expr << "\n";                      \
        }                                                                          \
    } while (0)

#pragma once

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <string>
#include <cstdint>

/// <summary>
/// 輕量型單元測試統計資料結構。
/// </summary>
struct TestStats {
    int32_t total{0};
    int32_t passed{0};
    int32_t failed{0};
};

/// <summary>
/// 取得全域測試統計參考。
/// </summary>
/// <returns>全域 TestStats 參考</returns>
inline TestStats& GetGlobalTestStats() noexcept {
    static TestStats stats;
    return stats;
}

#define TEST_ASSERT(cond) \
    do { \
        GetGlobalTestStats().total++; \
        if (cond) { \
            GetGlobalTestStats().passed++; \
        } else { \
            GetGlobalTestStats().failed++; \
            std::cerr << "[FAIL] Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (false)

#define TEST_ASSERT_THROWS(expr, ExceptionType) \
    do { \
        GetGlobalTestStats().total++; \
        bool threw = false; \
        try { \
            (void)(expr); \
        } catch (const ExceptionType&) { \
            threw = true; \
        } catch (...) { \
            GetGlobalTestStats().failed++; \
            std::cerr << "[FAIL] Expected " #ExceptionType " but caught unexpected exception type at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
        if (threw) { \
            GetGlobalTestStats().passed++; \
        } else { \
            GetGlobalTestStats().failed++; \
            std::cerr << "[FAIL] Expected " #ExceptionType " was not thrown at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (false)

// filepath: tests/test_main.cpp
/**
 * @file test_main.cpp
 * @brief Entry point executing all C++23 BigInt unit test suites.
 *        執行所有 C++23 BigInt 單元測試套件的進入點。
 */

#include "test_helpers.hpp"
#include <iostream>

auto run_test_bigint() -> void;
auto run_test_operators() -> void;
auto run_test_bitset() -> void;
auto run_test_constexpr() -> void;
auto run_test_bz_division() -> void;
auto run_test_toom3() -> void;

int main() {
    std::cout << std::unitbuf;
    std::cout << "========================================\n";
    std::cout << "  Starting CPP-BigInt Unit Test Suite   \n";
    std::cout << "========================================\n";

    run_test_bigint();
    run_test_operators();
    run_test_bitset();
    run_test_constexpr();
    run_test_bz_division();
    run_test_toom3();

    const auto& stats = numeric::test::GetGlobalTestStats();
    std::cout << "========================================\n";
    std::cout << "Test Results Summary:\n";
    std::cout << "  Total Assertions: " << stats.total << "\n";
    std::cout << "  Passed:           " << stats.passed << "\n";
    std::cout << "  Failed:           " << stats.failed << "\n";
    std::cout << "========================================\n";

    if (stats.failed > 0) {
        std::cerr << "[ERROR] Some tests failed!\n";
        return 1;
    }

    std::cout << "[SUCCESS] All tests passed!\n";
    return 0;
}

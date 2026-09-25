#include "test_helpers.hpp"
#include <iostream>

void run_test_bigint();
void run_test_bitset();
void run_test_constexpr();
void run_test_bz_division();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Starting CPP-BigInt Full Test Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    run_test_bigint();
    run_test_bitset();
    run_test_constexpr();
    run_test_bz_division();

    TestStats& stats = GetGlobalTestStats();
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Total Assertions: " << stats.total << std::endl;
    std::cout << "  Passed:           " << stats.passed << std::endl;
    std::cout << "  Failed:           " << stats.failed << std::endl;
    std::cout << "========================================" << std::endl;

    if (stats.failed == 0) {
        std::cout << "ALL BIGINT TESTS PASSED SUCCESSFULLY!" << std::endl;
        return 0;
    } else {
        std::cerr << "TEST SUITE FAILED WITH " << stats.failed << " ERRORS." << std::endl;
        return 1;
    }
}

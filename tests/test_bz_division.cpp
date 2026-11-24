// filepath: tests/test_bz_division.cpp
/**
 * @file test_bz_division.cpp
 * @brief Burnikel-Ziegler division and boundary unit tests.
 *        Burnikel-Ziegler 除法與邊界單元測試。
 */

#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

namespace {

// Construct a test BigInt with a specified limb count / 建立指定 limb 數的測試 BigInt
[[nodiscard]] auto make_limbs(std::size_t num_limbs,
                              std::uint64_t high_pattern,
                              std::mt19937_64& rng) -> numeric::bigint {
    if (num_limbs == 0U) {
        return numeric::bigint{0};
    }
    numeric::bigint res{0};
    for (std::size_t i = 0U; i < num_limbs; ++i) {
        const std::uint64_t word{(i == num_limbs - 1U) ? high_pattern : rng()};
        const numeric::bigint limb_val{word};
        res |= (limb_val << (i * 64U));
    }
    return res;
}

// Verify division identity: u == q * v + r and 0 <= r < v / 驗證除法恆等式
auto verify_div_identity(const numeric::bigint& u,
                         const numeric::bigint& v,
                         const std::string& case_name) -> void {
    const numeric::bigint q{u / v};
    const numeric::bigint r{u % v};

    TEST_ASSERT(r >= 0);
    TEST_ASSERT(r < v);

    const numeric::bigint reconstructed{q * v + r};
    const bool match{reconstructed == u};
    if (!match) {
        std::cerr << "FAIL in case: " << case_name << "\n";
        std::cerr << "  u limbs: " << u.limb_count() << ", v limbs: " << v.limb_count() << "\n";
    }
    TEST_ASSERT(match);
}

} // namespace

auto run_test_bz_division() -> void {
    std::cout << "--- Running Burnikel-Ziegler Division & Boundary Tests ---\n";

    std::mt19937_64 rng{123456789ULL};

    // 1. Boundary limb tests / 邊界 Limbs 測試
    constexpr std::array<std::size_t, 9> test_limbs{31U, 32U, 33U, 63U, 64U, 65U, 127U, 128U, 129U};
    for (const std::size_t v_limbs : test_limbs) {
        std::cout << "  Testing v_limbs = " << v_limbs << "\n";
        const std::uint64_t high_norm0{0x8000000000000000ULL | (rng() & 0x7FFFFFFFFFFFFFFFULL)};
        const std::uint64_t high_norm1{0x4000000000000000ULL | (rng() & 0x3FFFFFFFFFFFFFFFULL)};
        constexpr std::uint64_t high_norm63{1ULL};

        for (const std::uint64_t high_pat : {high_norm0, high_norm1, high_norm63}) {
            const numeric::bigint v{make_limbs(v_limbs, high_pat, rng)};
            if (v == 0) {
                continue;
            }

            {
                std::cout << "    Scene 1\n";
                const numeric::bigint u{make_limbs(v_limbs - 1U, 0x8000000000000000ULL, rng)};
                verify_div_identity(u, v, "u < v (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u / v == 0);
                TEST_ASSERT(u % v == u);
            }

            {
                std::cout << "    Scene 2\n";
                const numeric::bigint u{v + static_cast<std::uint64_t>((rng() % 1000ULL) + 1ULL)};
                verify_div_identity(u, v, "u ≈ v (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u / v == 1);
            }

            {
                std::cout << "    Scene 3\n";
                const numeric::bigint u{make_limbs(v_limbs * 2U, 0x8000000000000000ULL, rng)};
                verify_div_identity(u, v, "u ≈ 2v (v_limbs=" + std::to_string(v_limbs) + ")");
            }

            {
                std::cout << "    Scene 4\n";
                const numeric::bigint u{
                    make_limbs((v_limbs * 3U) + (v_limbs / 2U), 0x8000000000000000ULL, rng)};
                verify_div_identity(
                    u, v, "u multi-block (v_limbs=" + std::to_string(v_limbs) + ")");
            }

            {
                std::cout << "    Scene 5\n";
                const numeric::bigint q_exact{static_cast<std::uint64_t>((rng() % 1000000ULL) + 2ULL)};
                const numeric::bigint u_exact{q_exact * v};
                verify_div_identity(
                    u_exact, v, "Div_Exact (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u_exact % v == 0);
                TEST_ASSERT(u_exact / v == q_exact);
            }
        }
    }

    // 2. Randomized large differential tests / 隨機差分測試
    for (std::int32_t rep = 0; rep < 20; ++rep) {
        const std::size_t v_len{64U + static_cast<std::size_t>(rng() % 70ULL)};
        const std::size_t u_len{v_len + static_cast<std::size_t>(rng() % 100ULL)};
        const std::uint64_t v_hi{rng() | 1ULL};
        const std::uint64_t u_hi{rng() | 1ULL};
        const numeric::bigint v{make_limbs(v_len, v_hi, rng)};
        const numeric::bigint u{make_limbs(u_len, u_hi, rng)};
        verify_div_identity(u, v, "Random Large Div (rep=" + std::to_string(rep) + ")");
    }

    std::cout << "--- Burnikel-Ziegler Division Tests Completed Successfully ---\n";
}

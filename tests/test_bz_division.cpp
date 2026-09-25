#include "test_helpers.hpp"
#include <numeric/BigInt.hpp>
#include <cstdint>
#include <random>
#include <vector>
#include <string>

namespace {

// 建立指定 limb 數的測試 BigInt
numeric::bigint make_limbs(size_t num_limbs, uint64_t high_pattern, std::mt19937_64& rng) {
    if (num_limbs == 0) return numeric::bigint(0);
    numeric::bigint res(0);
    for (size_t i = 0; i < num_limbs; ++i) {
        uint64_t word = (i == num_limbs - 1) ? high_pattern : rng();
        numeric::bigint limb_val(word);
        res |= (limb_val << (i * 64));
    }
    return res;
}

// 驗證除法恆等式：u == q * v + r 且 0 <= r < v
void verify_div_identity(const numeric::bigint& u, const numeric::bigint& v, const std::string& case_name) {
    numeric::bigint q = u / v;
    numeric::bigint r = u % v;

    // 0 <= r < v
    TEST_ASSERT(r >= 0);
    TEST_ASSERT(r < v);

    // u == q * v + r
    numeric::bigint reconstructed = q * v + r;
    bool match = (reconstructed == u);
    if (!match) {
        std::cerr << "FAIL in case: " << case_name << "\n";
        std::cerr << "  u limbs: " << u.limb_count() << ", v limbs: " << v.limb_count() << "\n";
    }
    TEST_ASSERT(match);
}

} // namespace

void run_test_bz_division() {
    std::cout << "--- Running Burnikel-Ziegler Division & Boundary Tests ---" << std::endl;

    std::mt19937_64 rng(123456789ULL);

    // 1. 邊界 Limbs 測試 (63, 64, 65 limbs; 127, 128, 129 limbs)
    const size_t test_limbs[] = { 63, 64, 65, 127, 128, 129 };
    for (size_t v_limbs : test_limbs) {
        std::cout << "  Testing v_limbs = " << v_limbs << std::endl;
        // 最高 bit 為 1（norm shift = 0）
        uint64_t high_norm0 = 0x8000000000000000ULL | (rng() & 0x7FFFFFFFFFFFFFFFULL);
        // 最高 bit 為 0（norm shift = 1）
        uint64_t high_norm1 = 0x4000000000000000ULL | (rng() & 0x3FFFFFFFFFFFFFFFULL);
        // 最高 bit 僅低 1 位（norm shift = 63）
        uint64_t high_norm63 = 1ULL;

        for (uint64_t high_pat : { high_norm0, high_norm1, high_norm63 }) {
            numeric::bigint v = make_limbs(v_limbs, high_pat, rng);
            if (v == 0) continue;

            // 情境 1: u < v
            {
                std::cout << "    Scene 1" << std::endl;
                numeric::bigint u = make_limbs(v_limbs - 1, 0x8000000000000000ULL, rng);
                verify_div_identity(u, v, "u < v (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u / v == 0);
                TEST_ASSERT(u % v == u);
            }

            // 情境 2: u ≈ v
            {
                std::cout << "    Scene 2" << std::endl;
                numeric::bigint u = v + (rng() % 1000 + 1);
                verify_div_identity(u, v, "u ≈ v (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u / v == 1);
            }

            // 情境 3: u ≈ 2v (BZ 2x1 核心結構)
            {
                std::cout << "    Scene 3" << std::endl;
                numeric::bigint u = make_limbs(v_limbs * 2, 0x8000000000000000ULL, rng);
                verify_div_identity(u, v, "u ≈ 2v (v_limbs=" + std::to_string(v_limbs) + ")");
            }

            // 情境 4: Multi-block (u ≈ 3.5v)
            {
                std::cout << "    Scene 4" << std::endl;
                numeric::bigint u = make_limbs(v_limbs * 3 + v_limbs / 2, 0x8000000000000000ULL, rng);
                verify_div_identity(u, v, "u multi-block (v_limbs=" + std::to_string(v_limbs) + ")");
            }

            // 情境 5: 餘數剛好為 0 (Div_Exact)
            {
                std::cout << "    Scene 5" << std::endl;
                numeric::bigint q_exact(rng() % 1000000 + 2);
                numeric::bigint u_exact = q_exact * v;
                verify_div_identity(u_exact, v, "Div_Exact (v_limbs=" + std::to_string(v_limbs) + ")");
                TEST_ASSERT(u_exact % v == 0);
                TEST_ASSERT(u_exact / v == q_exact);
            }
        }
    }

    // 2. 隨機差分測試：大量中大型亂數對測試
    for (int rep = 0; rep < 20; ++rep) {
        size_t v_len = 64 + (rng() % 70); // 64 ~ 133 limbs (4096 ~ 8512 bits)
        size_t u_len = v_len + (rng() % 100); // v_len ~ v_len + 99 limbs
        uint64_t v_hi = rng() | 1ULL; // 至少非 0
        uint64_t u_hi = rng() | 1ULL;
        numeric::bigint v = make_limbs(v_len, v_hi, rng);
        numeric::bigint u = make_limbs(u_len, u_hi, rng);
        verify_div_identity(u, v, "Random Large Div (rep=" + std::to_string(rep) + ")");
    }

    std::cout << "--- Burnikel-Ziegler Division Tests Completed Successfully ---" << std::endl;
}

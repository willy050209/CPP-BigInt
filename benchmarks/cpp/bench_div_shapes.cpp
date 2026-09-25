#include <numeric/BigInt.hpp>
#include "../include/bench_common.hpp"
#include <iostream>
#include <vector>
#include <random>

numeric::bigint make_limbs(size_t num_limbs, std::mt19937_64& rng) {
    if (num_limbs == 0) return numeric::bigint(0);
    numeric::bigint res(0);
    for (size_t i = 0; i < num_limbs; ++i) {
        uint64_t word = (i == num_limbs - 1) ? (0x8000000000000000ULL | (rng() & 0x7FFFFFFFFFFFFFFFULL)) : rng();
        numeric::bigint limb_val(word);
        res |= (limb_val << (i * 64));
    }
    return res;
}

int main() {
    std::cout << "Comparing Knuth vs Burnikel-Ziegler 2x1 Division (u = 2N limbs, v = N limbs)...\n";
    std::mt19937_64 rng(42);

    const size_t sizes[] = { 64, 96, 128, 160, 192, 256, 384, 512 };

    for (size_t n : sizes) {
        std::vector<numeric::bigint> u_list, v_list;
        size_t samples = (n <= 128) ? 30 : 10;
        for (size_t i = 0; i < samples; ++i) {
            v_list.push_back(make_limbs(n, rng));
            u_list.push_back(make_limbs(2 * n, rng));
        }

        // Measure Knuth
        double elapsed_knuth = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                numeric::detail::BigIntStorage q, r;
                numeric::detail::BigIntCore::div_mod_core_knuth(&q, &r, u_list[i].storage(), v_list[i].storage());
                bench::do_not_optimize(q);
            }
        }, 3);

        // Measure Current div_mod_core (BZ)
        double elapsed_dispatch = bench::measure_time_ns([&]() {
            for (size_t i = 0; i < samples; ++i) {
                auto q = u_list[i] / v_list[i];
                bench::do_not_optimize(q);
            }
        }, 3);

        double us_knuth = (elapsed_knuth / (3 * samples)) / 1000.0;
        double us_dispatch = (elapsed_dispatch / (3 * samples)) / 1000.0;
        double speedup = us_knuth / us_dispatch;
        std::cout << "N = " << n << " limbs (" << n * 64 << " bits): "
                  << "Knuth = " << us_knuth << " us | "
                  << "BZ = " << us_dispatch << " us | "
                  << "Ratio = " << speedup << "x\n";
    }

    return 0;
}

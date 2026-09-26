#!/usr/bin/env python3
"""
bench_python.py
Benchmarks Python 3.12 built-in int and gmpy2 (GMP wrapper) across small, medium, and large tiers.
"""

import os
import sys
import time
import json

if hasattr(sys, "set_int_max_str_digits"):
    sys.set_int_max_str_digits(1000000)

try:
    import gmpy2
    HAVE_GMPY2 = True
except ImportError:
    HAVE_GMPY2 = False

def load_dataset(file_path):
    pairs = []
    if not os.path.exists(file_path):
        return pairs
    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 4:
                pairs.append((parts[0], parts[1], parts[2], parts[3]))
    return pairs

def measure_ns(func, iterations, warmup=5):
    for _ in range(warmup):
        func()
    start = time.perf_counter_ns()
    for _ in range(iterations):
        func()
    end = time.perf_counter_ns()
    return end - start

def run_suite(target_name, use_gmpy, data_dir, out_json):
    print("========================================================================")
    print(f"        Benchmark Target: {target_name}                                 ")
    print("========================================================================")

    metrics = []

    def run_tier(tier, bits, arith_iters, mul_div_iters, io_iters, mem_iters):
        path = os.path.join(data_dir, f"{tier}_{bits}.txt")
        pairs = load_dataset(path)
        if not pairs:
            return

        N = len(pairs)
        if use_gmpy:
            a_nums = [gmpy2.mpz(p[0]) for p in pairs]
            b_nums = [gmpy2.mpz(p[1]) for p in pairs]
        else:
            a_nums = [int(p[0]) for p in pairs]
            b_nums = [int(p[1]) for p in pairs]

        def record(op, iters, total_ns):
            total_ops = iters * N
            ns_per_op = total_ns / total_ops
            ops_per_sec = (total_ops * 1e9 / total_ns) if total_ns > 0 else 0
            metrics.append({
                "tier": tier,
                "bits": bits,
                "operation": op,
                "iterations": total_ops,
                "total_ns": total_ns,
                "ns_per_op": ns_per_op,
                "ops_per_sec": ops_per_sec
            })
            print(f"{target_name:<22} {tier:<8} {bits:>6} {op:<16} {ns_per_op:>12.2f} ns/op {ops_per_sec:>14,.0f} ops/s")

        # 1. Add (Fresh Return)
        def test_add():
            sink = 0
            for i in range(N):
                c = a_nums[i] + b_nums[i]
                sink += (c > 0)
        record("Add", arith_iters, measure_ns(test_add, arith_iters))

        # 1b. Add In-Place
        def test_add_inplace():
            sink = 0
            a_copy = list(a_nums)
            for i in range(N):
                a_copy[i] += b_nums[i]
                sink += (a_copy[i] > 0)
        record("Add_InPlace", arith_iters, measure_ns(test_add_inplace, arith_iters))

        # 2. Sub
        def test_sub():
            sink = 0
            for i in range(N):
                c = a_nums[i] - b_nums[i]
                sink += (c > 0)
        record("Sub", arith_iters, measure_ns(test_sub, arith_iters))

        # 3. Mul
        def test_mul():
            sink = 0
            for i in range(N):
                c = a_nums[i] * b_nums[i]
                sink += (c > 0)
        record("Mul", mul_div_iters, measure_ns(test_mul, mul_div_iters))

        # 4. Div
        def test_div():
            sink = 0
            for i in range(N):
                c = a_nums[i] // b_nums[i]
                sink += (c > 0)
        record("Div", mul_div_iters, measure_ns(test_div, mul_div_iters))

        # 5. Mod
        def test_mod():
            sink = 0
            for i in range(N):
                c = a_nums[i] % b_nums[i]
                sink += (c > 0)
        record("Mod", mul_div_iters, measure_ns(test_mod, mul_div_iters))

        # 6. ToString_10
        def test_to_str():
            sink = 0
            for i in range(N):
                sink += len(str(a_nums[i]))
        record("ToString_10", io_iters, measure_ns(test_to_str, io_iters))

        # 7. FromString_10
        if use_gmpy:
            def test_from_str():
                sink = 0
                for i in range(N):
                    val = gmpy2.mpz(pairs[i][0])
                    sink += (val > 0)
            record("FromString_10", io_iters, measure_ns(test_from_str, io_iters))
        else:
            def test_from_str():
                sink = 0
                for i in range(N):
                    val = int(pairs[i][0])
                    sink += (val > 0)
            record("FromString_10", io_iters, measure_ns(test_from_str, io_iters))

        # 8. MemPressure: temporary variable chained operations
        def test_mem():
            sink = 0
            for i in range(N):
                tmp = (a_nums[i] + b_nums[i]) - (a_nums[i] ^ b_nums[i])
                sink += (tmp > 0)
        record("MemPressure", mem_iters, measure_ns(test_mem, mem_iters))

        # 9. Chained temporary expression: (a + b) * (a - b)
        def test_chained():
            sink = 0
            for i in range(N):
                d = (a_nums[i] + b_nums[i]) * (a_nums[i] - b_nums[i])
                sink += (d > 0)
        record("Chained_Expr", mul_div_iters, measure_ns(test_chained, mul_div_iters))

    # Small
    run_tier("small", 64,  50, 50, 20, 50)
    run_tier("small", 128, 50, 50, 20, 50)
    run_tier("small", 256, 50, 50, 20, 50)

    # Medium
    run_tier("medium", 512,  20, 20, 10, 20)
    run_tier("medium", 1024, 20, 20, 10, 20)
    run_tier("medium", 2048, 10, 10, 5,  10)
    run_tier("medium", 4096, 5,  5,  2,  5)

    # Large
    run_tier("large", 16384, 3, 2, 1, 2)
    run_tier("large", 32768, 2, 1, 1, 1)
    run_tier("large", 65536, 1, 1, 1, 1)

    os.makedirs(os.path.dirname(out_json), exist_ok=True)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump({"target": target_name, "metrics": metrics}, f, indent=2)
    print(f"[{target_name} Benchmark] Results exported to {out_json}")

if __name__ == "__main__":
    data_dir = sys.argv[1] if len(sys.argv) > 1 else "benchmarks/data"
    res_dir = sys.argv[2] if len(sys.argv) > 2 else "benchmarks/results"

    # 1. Benchmark Python native int
    run_suite("Python 3.12 (int)", False, data_dir, os.path.join(res_dir, "results_python.json"))

    # 2. Benchmark Python gmpy2 (GMP wrapper)
    if HAVE_GMPY2:
        run_suite("Python (gmpy2/GMP)", True, data_dir, os.path.join(res_dir, "results_gmpy2.json"))

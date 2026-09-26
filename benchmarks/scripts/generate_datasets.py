#!/usr/bin/env python3
"""
generate_datasets.py
Generates deterministic pseudo-random datasets for cross-language BigInt benchmarks.
Fixed random seed ensures identical test inputs across C++, .NET, and Python.
"""

import json
import random
import os
import sys

if hasattr(sys, "set_int_max_str_digits"):
    sys.set_int_max_str_digits(1000000)

SEED = 42

CONFIG = {
    "small": {
        "bits": [64, 128, 256],
        "count": 500
    },
    "medium": {
        "bits": [512, 1024, 2048, 4096],
        "count": 100
    },
    "large": {
        "bits": [16384, 32768, 65536],
        "count": 10
    },
    "sweep": {
        "bits": [64, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 32768, 65536],
        "count": 20
    }
}

def generate_random_int_with_exact_bits(rng, bits):
    """Generate a random positive integer with exact bit length (highest bit = 1)."""
    if bits == 1:
        return 1
    # Ensure highest bit is 1 so bit length is exact
    val = (1 << (bits - 1)) | rng.getrandbits(bits - 1)
    return val

def generate_all():
    rng = random.Random(SEED)
    dataset = {
        "seed": SEED,
        "tiers": {}
    }

    print(f"[Dataset Generator] Generating deterministic test vectors (seed={SEED})...")

    for tier_name, tier_info in CONFIG.items():
        dataset["tiers"][tier_name] = {}
        for bits in tier_info["bits"]:
            count = tier_info["count"]
            pairs = []
            for _ in range(count):
                a = generate_random_int_with_exact_bits(rng, bits)
                # For b, make it either equal bits or slightly smaller to test varied division
                b_bits = rng.randint(max(1, bits // 2), bits)
                b = generate_random_int_with_exact_bits(rng, b_bits)
                if b == 0:
                    b = 1
                pairs.append({
                    "a_dec": str(a),
                    "b_dec": str(b),
                    "a_hex": hex(a)[2:],
                    "b_hex": hex(b)[2:],
                    "a_bits": a.bit_length(),
                    "b_bits": b.bit_length()
                })
            dataset["tiers"][tier_name][str(bits)] = pairs
            print(f"  - Generated {count} pairs for {tier_name} tier: {bits} bits")

    out_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "data")
    os.makedirs(out_dir, exist_ok=True)
    out_file = os.path.join(out_dir, "dataset.json")

    with open(out_file, "w", encoding="utf-8") as f:
        json.dump(dataset, f, indent=2)

    # Also output flat text files for easy 0-dependency loading in C++
    for tier_name, tier_dict in dataset["tiers"].items():
        for bits_str, pairs in tier_dict.items():
            txt_path = os.path.join(out_dir, f"{tier_name}_{bits_str}.txt")
            with open(txt_path, "w", encoding="utf-8") as tf:
                for p in pairs:
                    tf.write(f"{p['a_dec']} {p['b_dec']} {p['a_hex']} {p['b_hex']}\n")

    size_mb = os.path.getsize(out_file) / (1024 * 1024)
    print(f"[Dataset Generator] Saved dataset to {out_file} ({size_mb:.2f} MB) and plain-text files.")
    return out_file

if __name__ == "__main__":
    generate_all()

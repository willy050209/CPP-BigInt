import json
import sys

def main():
    with open('benchmarks/results/results_cpp_bigint.json') as f:
        cpp = json.load(f)['metrics']
    with open('benchmarks/results/results_cpp_gmp.json') as f:
        gmp = json.load(f)['metrics']

    gmp_map = {(m['tier'], m['bits'], m['operation']): m['ns_per_op'] for m in gmp}
    
    comparisons = []
    for m in cpp:
        key = (m['tier'], m['bits'], m['operation'])
        if key in gmp_map:
            cpp_ns = m['ns_per_op']
            gmp_ns = gmp_map[key]
            ratio = gmp_ns / cpp_ns if cpp_ns > 0 else 0
            comparisons.append({
                'tier': m['tier'],
                'bits': m['bits'],
                'op': m['operation'],
                'cpp_ns': cpp_ns,
                'gmp_ns': gmp_ns,
                'speedup': ratio, # > 1.0 means CPP is faster; < 1.0 means GMP is faster
                'gap_ns': cpp_ns - gmp_ns
            })

    # Filter where CPP is slower than GMP (speedup < 1.0)
    slower = [c for c in comparisons if c['speedup'] < 0.95]
    slower.sort(key=lambda x: x['speedup']) # worst ratio first

    print(f"Total metrics compared: {len(comparisons)}")
    print(f"Metrics where CPP-BigInt is slower than GMP (< 0.95x): {len(slower)}\n")
    print(f"{'Bits':<6} | {'Operation':<16} | {'CPP (ns)':<10} | {'GMP (ns)':<10} | {'Speedup':<8} | {'Gap (ns)':<10}")
    print("-" * 72)
    for c in slower:
        print(f"{c['bits']:<6} | {c['op']:<16} | {c['cpp_ns']:<10.2f} | {c['gmp_ns']:<10.2f} | {c['speedup']:<7.2f}x | {c['gap_ns']:<10.2f}")

if __name__ == '__main__':
    main()

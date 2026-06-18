import time
import os
import numpy as np
from gbm_simulation import (
    price_european_call,
    price_european_call_parallel,
    price_asian_call,
    price_asian_call_parallel,
    price_barrier_call,
    price_barrier_call_parallel
)

def run_threshold_analysis():
    # --- Fixed Evaluation Parameters ---
    S0, K, r, sigma, T = 100.0, 100.0, 0.05, 0.2, 1.0
    B = 120.0
    n_steps = 100  # Fixed path steps for Asian/Barrier options
    n_workers = os.cpu_count() or 4
    
    # Research-relevant range of simulations (Logarithmic scaling)
    n_sims_range = [1_000, 5_000, 10_000, 25_000, 50_000, 100_000, 250_000, 500_000, 1_000_000, 2_000_000]

    # Configuration for the three types of options
    options_config = {
        "European Call": {
            "seq_func": lambda n: price_european_call(S0, r, sigma, T, K, n),
            "par_func": lambda n: price_european_call_parallel(S0, r, sigma, T, K, n, n_workers=n_workers)
        },
        "Asian Call": {
            "seq_func": lambda n: price_asian_call(S0, r, sigma, T, K, n_steps, n),
            "par_func": lambda n: price_asian_call_parallel(S0, r, sigma, T, K, n_steps, n, n_workers=n_workers)
        },
        "Barrier Call": {
            "seq_func": lambda n: price_barrier_call(S0, r, sigma, T, K, n_steps, n, B),
            "par_func": lambda n: price_barrier_call_parallel(S0, r, sigma, T, K, B, n_steps, n, n_workers=n_workers)
        }
    }

    print("=" * 75)
    print(f"MULTIPROCESSING CROSSOVER BENCHMARK (Workers: {n_workers})")
    print("=" * 75)
    print("Warming up process pool to eliminate cold-start bias...")
    _ = price_european_call_parallel(S0, r, sigma, T, K, 10_000, n_workers=n_workers)
    print("Warm-up complete. Starting evaluation...\n")

    summary_n0 = {}

    for name, funcs in options_config.items():
        print(f"Evaluating Performance Profile: {name}")
        print(f"{'-'*75}")
        print(f"{'n_sims':>12} | {'Seq Time (s)':>12} | {'Par Time (s)':>12} | {'Speedup':>10} | {'Efficiency':>10}")
        print(f"{'-'*75}")

        crossover_found = False
        n0_val = "Not reached (> 2M)"

        for n_sims in n_sims_range:
            # Time Sequential Execution
            t0 = time.perf_counter()
            funcs["seq_func"](n_sims)
            t_seq = time.perf_counter() - t0

            # Time Parallel Execution
            t0 = time.perf_counter()
            funcs["par_func"](n_sims)
            t_par = time.perf_counter() - t0

            # Metric Calculations
            speedup = t_seq / t_par
            efficiency = speedup / n_workers

            print(f"{n_sims:12,} | {t_seq:12.5f} | {t_par:12.5f} | {speedup:10.2f}x | {efficiency:10.2%}")

            # Identify first instance where parallel outperforms sequential
            if t_par < t_seq and not crossover_found:
                n0_val = f"{n_sims:,}"
                crossover_found = True

        summary_n0[name] = n0_val
        print(f"{'-'*75}")
        print(f"Conclusion for {name}: Crossover Threshold (n₀) ≈ {n0_val}\n\n")

    # Final Summary Table
    print("=" * 50)
    print("FINAL SUMMARY: CROSSOVER THRESHOLDS (n₀)")
    print("=" * 50)
    for option_type, threshold in summary_n0.items():
        print(f" {option_type:<20} : Parallel is better when n_sims >= {threshold}")
    print("=" * 50)

if __name__ == '__main__':
    run_threshold_analysis()
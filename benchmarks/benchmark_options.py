import time
import os
from gbm_simulation import (
    price_european_call,
    price_european_call_parallel,
    price_asian_call,
    price_asian_call_parallel,
    price_barrier_call,
    price_barrier_call_parallel,
    black_scholes_call
)

def benchmark():
    # --- Benchmark Parameters ---
    S0, K, r, sigma, T = 100.0, 100.0, 0.05, 0.2, 1.0
    B = 120.0
    n_steps = 252 # Daily steps for a year
    
    # Use heavy workloads to see the parallelization benefits
    n_sims_euro = 5_000_0000
    n_sims_path = 200_000
    
    n_workers = os.cpu_count() or 4

    print(f"{'='*50}")
    print(f"MONTE CARLO PRICER BENCHMARK (Workers: {n_workers})")
    print(f"{'='*50}\n")

    # 1. European Call
    print(f"--- European Call ({n_sims_euro:,} simulations) ---")
    t0 = time.perf_counter()
    bs_price = black_scholes_call(S0, r, sigma, T, K)
    t1 = time.perf_counter()
    print(f"Exact Black-Scholes : {bs_price:.4f} | Time: {t1-t0:.4f}s")

    t0 = time.perf_counter()
    seq_euro = price_european_call(S0, r, sigma, T, K, n_sims_euro)
    t1 = time.perf_counter()
    seq_time = t1 - t0
    print(f"Sequential MC       : {seq_euro:.4f} | Time: {seq_time:.4f}s")

    t0 = time.perf_counter()
    par_euro = price_european_call_parallel(S0, r, sigma, T, K, n_sims_euro, n_workers=n_workers)
    t1 = time.perf_counter()
    par_time = t1 - t0
    print(f"Parallel MC         : {par_euro:.4f} | Time: {par_time:.4f}s")
    print(f"-> Parallel Speedup : {seq_time / par_time:.2f}x\n")

    # 2. Asian Call
    print(f"--- Asian Call ({n_sims_path:,} simulations, {n_steps} steps) ---")
    t0 = time.perf_counter()
    seq_asian = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims_path)
    t1 = time.perf_counter()
    seq_time = t1 - t0
    print(f"Sequential MC       : {seq_asian:.4f} | Time: {seq_time:.4f}s")

    t0 = time.perf_counter()
    par_asian = price_asian_call_parallel(S0, r, sigma, T, K, n_steps, n_sims_path, n_workers=n_workers)
    t1 = time.perf_counter()
    par_time = t1 - t0
    print(f"Parallel MC         : {par_asian:.4f} | Time: {par_time:.4f}s")
    print(f"-> Parallel Speedup : {seq_time / par_time:.2f}x\n")

    # 3. Barrier Call
    print(f"--- Up-and-Out Barrier Call ({n_sims_path:,} simulations, {n_steps} steps) ---")
    t0 = time.perf_counter()
    seq_bar = price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims_path, B)
    t1 = time.perf_counter()
    seq_time = t1 - t0
    print(f"Sequential MC       : {seq_bar:.4f} | Time: {seq_time:.4f}s")

    t0 = time.perf_counter()
    par_bar = price_barrier_call_parallel(S0, r, sigma, T, K, B, n_steps, n_sims_path, n_workers=n_workers)
    t1 = time.perf_counter()
    par_time = t1 - t0
    print(f"Parallel MC         : {par_bar:.4f} | Time: {par_time:.4f}s")
    print(f"-> Parallel Speedup : {seq_time / par_time:.2f}x\n")

if __name__ == '__main__':
    # Running inside __main__ is necessary for multiprocessing on Windows
    benchmark()
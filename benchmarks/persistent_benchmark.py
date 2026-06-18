import time
import os
import numpy as np
from multiprocessing import Pool
from gbm_simulation import price_european_call, worker_european_call, chunk_sizes

# Refactored function that accepts an existing, long-lived process pool
def price_european_call_shared_pool(S0, r, sigma, T, K, n_sims, pool, chunk_size=200_000, seed=1, antithetic=True):
    chunks = chunk_sizes(n_sims, chunk_size)

    seed_seq = np.random.SeedSequence(seed)
    child_seqs = seed_seq.spawn(len(chunks))

    args_list = [
        (S0, r, sigma, T, K, chunks[i], child_seqs[i], antithetic)
        for i in range(len(chunks))
    ]

    # Use the existing pool instead of a 'with Pool(...) as pool' block
    results = pool.map(worker_european_call, args_list)

    total_payoff = sum(s for s, _ in results)
    total_count = sum(c for _, c in results)

    return np.exp(-r * T) * (total_payoff / total_count)

# Ephemeral pool function (simulating your original implementation for direct comparison)
def price_european_call_ephemeral_pool(S0, r, sigma, T, K, n_sims, n_workers, chunk_size=200_000, seed=1, antithetic=True):
    chunks = chunk_sizes(n_sims, chunk_size)
    seed_seq = np.random.SeedSequence(seed)
    child_seqs = seed_seq.spawn(len(chunks))
    args_list = [(S0, r, sigma, T, K, chunks[i], child_seqs[i], antithetic) for i in range(len(chunks))]
    
    with Pool(processes=n_workers) as pool:
        results = pool.map(worker_european_call, args_list)
        
    return np.exp(-r * T) * (sum(s for s, _ in results) / sum(c for _, c in results))


def run_persistent_analysis():
    S0, K, r, sigma, T = 100.0, 100.0, 0.05, 0.2, 1.0
    n_workers = os.cpu_count() or 4
    
    # Testing the lower range where the ephemeral pool completely failed
    n_sims_range = [5_000, 25_000, 50_000, 100_000, 250_000, 500_000, 1_000_000]

    # Initialize the persistent pool once at the start of our application lifetime
    print("=" * 85)
    print(f"COMPARING EPHEMERAL VS. PERSISTENT PROCESS POOLS (Workers: {n_workers})")
    print("=" * 85)
    
    with Pool(processes=n_workers) as persistent_pool:
        # Warm up the persistent pool
        _ = price_european_call_shared_pool(S0, r, sigma, T, K, 10_000, persistent_pool)

        print(f"{'n_sims':>10} | {'Sequential (s)':>15} | {'Ephemeral Pool (s)':>20} | {'Persistent Pool (s)':>20}")
        print("-" * 85)

        for n_sims in n_sims_range:
            # 1. Sequential
            t0 = time.perf_counter()
            price_european_call(S0, r, sigma, T, K, n_sims)
            t_seq = time.perf_counter() - t0

            # 2. Ephemeral (Old Way)
            t0 = time.perf_counter()
            price_european_call_ephemeral_pool(S0, r, sigma, T, K, n_sims, n_workers=n_workers)
            t_eph = time.perf_counter() - t0

            # 3. Persistent (New Way)
            t0 = time.perf_counter()
            price_european_call_shared_pool(S0, r, sigma, T, K, n_sims, pool=persistent_pool)
            t_per = time.perf_counter() - t0

            print(f"{n_sims:10,} | {t_seq:15.5f} | {t_eph:20.5f} | {t_per:20.5f}")

    print("-" * 85)
    print("Observation: Notice how the Persistent Pool column avoids the fixed overhead penalty!")

if __name__ == '__main__':
    run_persistent_analysis()
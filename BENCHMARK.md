# Benchmark Report

This document records the current benchmark results for the Python Monte Carlo
options pricing engine.

The benchmark scripts were run after fixing their imports so they can be
executed directly from the repository root without manually setting
`PYTHONPATH`.

## Run Environment

```text
Date: 2026-06-18
Python: 3.14.4
Platform: Linux-7.0.5-arch1-1-x86_64-with-glibc2.43
CPU workers reported by os.cpu_count(): 14
NumPy: 2.4.6
SciPy: 1.17.1
Interpreter used: /tmp/options_bench_venv/bin/python
```

The benchmark scripts use Python `multiprocessing.Pool`, so they were executed
outside the restricted sandbox after multiprocessing failed inside the sandbox
with a forkserver permission error.

## Code Changes Needed Before Running

The benchmark files previously imported `gbm_simulation` directly:

```python
from gbm_simulation import ...
```

That only works if `src/` is already on `PYTHONPATH`. Each benchmark script now
adds the project `src/` directory to `sys.path` at startup:

```python
ROOT_DIR = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT_DIR / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
```

The main benchmark also had `n_sims_euro = 5_000_0000`, which is 50,000,000
simulations. This was corrected to `5_000_000` so the benchmark runs as the
comment describes: a heavy workload, but not an accidental 10x larger run.

## Benchmark 1: `benchmarks/benchmark_options.py`

Command:

```bash
/tmp/options_bench_venv/bin/python benchmarks/benchmark_options.py
```

Purpose:

- Compare analytical Black-Scholes pricing against Monte Carlo European call
  pricing.
- Compare sequential and multiprocessing implementations for European, Asian,
  and up-and-out barrier calls.
- Use one large European terminal-price workload and path-dependent workloads
  with daily time steps.

Parameters:

```text
S0 = 100.0
K = 100.0
r = 0.05
sigma = 0.2
T = 1.0
B = 120.0
n_steps = 252
n_sims_euro = 5,000,000
n_sims_path = 200,000
n_workers = 14
```

Output:

```text
==================================================
MONTE CARLO PRICER BENCHMARK (Workers: 14)
==================================================

--- European Call (5,000,000 simulations) ---
Exact Black-Scholes : 10.4506 | Time: 0.0006s
Sequential MC       : 10.4588 | Time: 0.1577s
Parallel MC         : 10.4556 | Time: 0.8534s
-> Parallel Speedup : 0.18x

--- Asian Call (200,000 simulations, 252 steps) ---
Sequential MC       : 5.7701 | Time: 1.8654s
Parallel MC         : 5.7676 | Time: 1.8815s
-> Parallel Speedup : 0.99x

--- Up-and-Out Barrier Call (200,000 simulations, 252 steps) ---
Sequential MC       : 1.3419 | Time: 1.8967s
Parallel MC         : 1.3235 | Time: 0.5865s
-> Parallel Speedup : 3.23x
```

Interpretation:

- The analytical Black-Scholes calculation is effectively instantaneous.
- For European calls, the sequential NumPy implementation is much faster than
  multiprocessing at this workload because the calculation is already highly
  vectorized and terminal-price-only.
- For Asian calls at 200,000 paths and 252 steps, sequential and parallel
  execution are almost identical in this run.
- For barrier calls at 200,000 paths and 252 steps, multiprocessing gives a
  significant speedup because each path requires full-path generation plus a
  barrier scan.

## Benchmark 2: `benchmarks/persistent_benchmark.py`

Command:

```bash
/tmp/options_bench_venv/bin/python benchmarks/persistent_benchmark.py
```

Purpose:

- Compare sequential European call pricing against two multiprocessing styles.
- Measure the overhead of creating a fresh process pool for each call.
- Measure the effect of reusing an existing process pool.

Parameters:

```text
S0 = 100.0
K = 100.0
r = 0.05
sigma = 0.2
T = 1.0
n_workers = 14
n_sims_range = [5,000, 25,000, 50,000, 100,000, 250,000, 500,000, 1,000,000]
```

Output:

```text
=====================================================================================
COMPARING EPHEMERAL VS. PERSISTENT PROCESS POOLS (Workers: 14)
=====================================================================================
    n_sims |  Sequential (s) |   Ephemeral Pool (s) |  Persistent Pool (s)
-------------------------------------------------------------------------------------
     5,000 |         0.00037 |              0.11011 |              0.00404
    25,000 |         0.00138 |              0.06957 |              0.00489
    50,000 |         0.00390 |              0.09101 |              0.00689
   100,000 |         0.00434 |              0.09865 |              0.00926
   250,000 |         0.01389 |              0.10267 |              0.01450
   500,000 |         0.04992 |              0.16302 |              0.01591
 1,000,000 |         0.04556 |              0.09727 |              0.01600
-------------------------------------------------------------------------------------
Observation: Notice how the Persistent Pool column avoids the fixed overhead penalty!
```

Interpretation:

- Creating a fresh process pool dominates runtime at smaller simulation counts.
- Reusing a process pool removes most of that fixed setup cost.
- For this European terminal-price-only workload, sequential NumPy remains very
  competitive because the per-simulation work is small.
- The timing at 500,000 and 1,000,000 simulations should be interpreted as a
  single-run measurement, not a statistically stable performance claim.

## Benchmark 3: `benchmarks/threshold_benchmark.py`

Command:

```bash
/tmp/options_bench_venv/bin/python benchmarks/threshold_benchmark.py
```

Purpose:

- Sweep simulation counts and estimate the crossover point where
  multiprocessing becomes faster than sequential execution.
- Evaluate European, Asian, and up-and-out barrier calls under the same market
  assumptions.

Parameters:

```text
S0 = 100.0
K = 100.0
r = 0.05
sigma = 0.2
T = 1.0
B = 120.0
n_steps = 100
n_workers = 14
n_sims_range = [
    1,000,
    5,000,
    10,000,
    25,000,
    50,000,
    100,000,
    250,000,
    500,000,
    1,000,000,
    2,000,000,
]
```

Output:

```text
===========================================================================
MULTIPROCESSING CROSSOVER BENCHMARK (Workers: 14)
===========================================================================
Warming up process pool to eliminate cold-start bias...
Warm-up complete. Starting evaluation...

Evaluating Performance Profile: European Call
---------------------------------------------------------------------------
      n_sims | Seq Time (s) | Par Time (s) |    Speedup | Efficiency
---------------------------------------------------------------------------
       1,000 |      0.00044 |      0.07251 |       0.01x |      0.04%
       5,000 |      0.00056 |      0.07996 |       0.01x |      0.05%
      10,000 |      0.00072 |      0.08263 |       0.01x |      0.06%
      25,000 |      0.00189 |      0.09516 |       0.02x |      0.14%
      50,000 |      0.00299 |      0.10540 |       0.03x |      0.20%
     100,000 |      0.00639 |      0.10569 |       0.06x |      0.43%
     250,000 |      0.01188 |      0.09994 |       0.12x |      0.85%
     500,000 |      0.02628 |      0.10283 |       0.26x |      1.83%
   1,000,000 |      0.06026 |      0.11329 |       0.53x |      3.80%
   2,000,000 |      0.08047 |      0.09595 |       0.84x |      5.99%
---------------------------------------------------------------------------
Conclusion for European Call: Crossover Threshold (n0) = Not reached (> 2M)


Evaluating Performance Profile: Asian Call
---------------------------------------------------------------------------
      n_sims | Seq Time (s) | Par Time (s) |    Speedup | Efficiency
---------------------------------------------------------------------------
       1,000 |      0.00832 |      0.09885 |       0.08x |      0.60%
       5,000 |      0.02798 |      0.13024 |       0.21x |      1.53%
      10,000 |      0.04493 |      0.14070 |       0.32x |      2.28%
      25,000 |      0.11084 |      0.18566 |       0.60x |      4.26%
      50,000 |      0.19838 |      0.30665 |       0.65x |      4.62%
     100,000 |      0.39406 |      0.49879 |       0.79x |      5.64%
     250,000 |      0.94814 |      0.85913 |       1.10x |      7.88%
     500,000 |      1.87606 |      0.84833 |       2.21x |     15.80%
   1,000,000 |      3.81165 |      1.20766 |       3.16x |     22.54%
   2,000,000 |      7.54908 |      1.98041 |       3.81x |     27.23%
---------------------------------------------------------------------------
Conclusion for Asian Call: Crossover Threshold (n0) = 250,000


Evaluating Performance Profile: Barrier Call
---------------------------------------------------------------------------
      n_sims | Seq Time (s) | Par Time (s) |    Speedup | Efficiency
---------------------------------------------------------------------------
       1,000 |      0.00515 |      0.07249 |       0.07x |      0.51%
       5,000 |      0.02674 |      0.10311 |       0.26x |      1.85%
      10,000 |      0.04704 |      0.12988 |       0.36x |      2.59%
      25,000 |      0.10397 |      0.23282 |       0.45x |      3.19%
      50,000 |      0.24336 |      0.31746 |       0.77x |      5.48%
     100,000 |      0.39911 |      0.27712 |       1.44x |     10.29%
     250,000 |      0.94911 |      0.33171 |       2.86x |     20.44%
     500,000 |      1.89188 |      0.46149 |       4.10x |     29.28%
   1,000,000 |      3.81059 |      0.88140 |       4.32x |     30.88%
   2,000,000 |      7.57271 |      1.44427 |       5.24x |     37.45%
---------------------------------------------------------------------------
Conclusion for Barrier Call: Crossover Threshold (n0) = 100,000


==================================================
FINAL SUMMARY: CROSSOVER THRESHOLDS (n0)
==================================================
 European Call        : Parallel is better when n_sims >= Not reached (> 2M)
 Asian Call           : Parallel is better when n_sims >= 250,000
 Barrier Call         : Parallel is better when n_sims >= 100,000
==================================================
```

Interpretation:

- European call pricing does not benefit from multiprocessing up to 2,000,000
  simulations in this benchmark. The terminal-price simulation is too cheap per
  path relative to process scheduling and inter-process overhead.
- Asian call pricing crosses over at roughly 250,000 simulations for this
  machine and parameter set.
- Barrier call pricing crosses over earlier, around 100,000 simulations,
  because each path requires both path generation and barrier monitoring.
- Worker efficiency remains well below ideal scaling. This is expected for
  Python multiprocessing with NumPy-heavy workloads, memory allocation, process
  coordination overhead, and chunk-level scheduling costs.

## Overall Conclusions

- The current benchmark data supports using sequential NumPy for small
  terminal-price European workloads.
- Multiprocessing becomes more useful for path-dependent options where each
  simulation does more work.
- Barrier calls show the strongest multiprocessing benefit in the current
  benchmark set.
- These are single-run timings. For publication-quality performance claims,
  each row should be repeated several times and reported with mean, standard
  deviation, and hardware details.


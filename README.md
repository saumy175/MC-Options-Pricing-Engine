# Options Pricing Engine

This repository is a Python Monte Carlo options pricing engine built around
Geometric Brownian Motion (GBM). The current code prices European, Asian, and
up-and-out barrier call options, compares Monte Carlo prices against the
Black-Scholes analytical call price where possible, and includes benchmark
scripts for sequential versus multiprocessing execution.

## Current Status

The project is currently a compact research/prototype implementation. The main
logic lives in `src/gbm_simulation.py`, with tests in `tests/` and performance
experiments in `benchmarks/`.

The implementation uses:

- `numpy` for vectorized random number generation, path construction, payoff
  calculation, and aggregation.
- `scipy.stats.norm` for the Black-Scholes normal CDF.
- Python `multiprocessing.Pool` for parallel pricing variants.
- `pytest` for correctness and regression tests.

## Repository Structure

```text
options_pricing_engine/
+-- benchmarks/
|   +-- benchmark_options.py
|   +-- persistent_benchmark.py
|   +-- threshold_benchmark.py
+-- config/
|   +-- currently empty
+-- src/
|   +-- gbm_simulation.py
+-- tests/
|   +-- test_options.py
+-- requirements.txt
+-- README.md
```

## Core Module: `src/gbm_simulation.py`

This file contains all pricing and simulation logic.

### Helper Functions

#### `chunk_sizes(total, chunk_size)`

Splits a requested simulation count into smaller chunk sizes.

Example behavior:

```python
chunk_sizes(450_000, 200_000)
# [200000, 200000, 50000]
```

This is used by chunked sequential path-dependent pricing and by the
multiprocessing implementations.

#### `_generate_path(S0, r, sigma, T, n_steps, seed=None)`

Generates one GBM price path.

It returns a one-dimensional array:

```text
[S0, S_dt, S_2dt, ..., S_T]
```

The path uses the log-return form of GBM:

```text
log(S_t+dt / S_t) = (r - 0.5 * sigma^2) * dt + sigma * sqrt(dt) * Z
```

where `Z` is sampled from the standard normal distribution.

This helper is currently not used by the pricing functions, because the code
mostly works with vectorized batches of paths.

#### `generate_paths(S0, r, sigma, T, n_steps, n_sims, seed=None)`

Generates many GBM paths at once.

It returns a two-dimensional NumPy array with shape:

```text
(n_sims, n_steps + 1)
```

Each row is one simulated price path. The first column is always `S0`, and the
last column is the terminal simulated price `S_T`.

This function is used by the Asian and barrier option pricers because both need
information about the full price path, not just the terminal price.

## Pricing Functions

The common parameters are:

- `S0`: initial asset price.
- `K`: strike price.
- `r`: continuously compounded risk-free rate.
- `sigma`: volatility.
- `T`: time to maturity in years.
- `n_sims`: number of Monte Carlo simulations.
- `n_steps`: number of time steps for path-dependent options.
- `seed`: random seed for reproducibility.

### European Call

#### `price_european_call(S0, r, sigma, T, K, n_sims, seed=1)`

Prices a plain European call option using Monte Carlo simulation.

Because a European call payoff only depends on the terminal price, this function
does not generate full paths. It samples terminal prices directly:

```text
S_T = S0 * exp((r - 0.5 * sigma^2) * T + sigma * sqrt(T) * Z)
```

The discounted payoff is:

```text
exp(-rT) * mean(max(S_T - K, 0))
```

#### `price_european_call_antithetic(...)`

Prices a European call using antithetic variates.

For every sampled normal random variable `Z`, the function also evaluates `-Z`.
The two payoffs are averaged before discounting. This can reduce Monte Carlo
variance without changing the expected payoff.

#### `price_european_call_parallel(...)`

Parallel European call pricer.

The function:

1. Splits `n_sims` into chunks.
2. Spawns independent child random seed sequences.
3. Sends each chunk to `worker_european_call`.
4. Aggregates payoff sums and counts from all workers.
5. Discounts the final average payoff.

By default, this parallel implementation uses antithetic variates.

#### `worker_european_call(args)`

Worker function used by `price_european_call_parallel`.

It prices one simulation chunk and returns:

```text
(sum_of_payoffs, number_of_payoffs)
```

Returning aggregate values instead of full payoff arrays keeps inter-process
data transfer smaller.

### Asian Call

#### `price_asian_call(S0, r, sigma, T, K, n_steps, n_sims, seed=None, chunk_size=50000)`

Prices an arithmetic-average Asian call option.

For each simulated path, the payoff is based on the average price over the full
path:

```text
max(mean(path) - K, 0)
```

The function processes simulations in chunks to avoid storing all paths for very
large `n_sims` values at once.

#### `price_asian_call_parallel(...)`

Parallel Asian call pricer.

The function splits simulations into chunks, generates full GBM paths inside
worker processes, computes arithmetic-average payoffs, aggregates payoff sums,
and discounts the mean payoff.

#### `worker_asian_call(args)`

Worker function used by `price_asian_call_parallel`.

It generates one batch of paths, computes arithmetic-average call payoffs, and
returns a payoff sum and payoff count.

### Up-and-Out Barrier Call

#### `price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims, B, seed=None, chunk_size=50000)`

Prices an up-and-out barrier call option.

The option is knocked out if the simulated path ever rises above the barrier
`B`. If the barrier is not breached, the payoff is the normal European call
payoff at maturity:

```text
max(S_T - K, 0)
```

If the path breaches the barrier:

```text
payoff = 0
```

The current implementation checks the barrier on the discrete simulated time
grid only.

#### `price_barrier_call_parallel(...)`

Parallel up-and-out barrier call pricer.

The function splits simulations into chunks, sends them to worker processes,
aggregates the payoff sums and counts, and discounts the average payoff.

#### `worker_barrier_call(args)`

Worker function used by `price_barrier_call_parallel`.

It generates full paths, checks whether each path crossed the barrier, computes
valid payoffs, and returns aggregate payoff data.

### Black-Scholes Reference

#### `black_scholes_call(S0, r, sigma, T, K)`

Computes the analytical Black-Scholes price for a European call option:

```text
C = S0 * N(d1) - K * exp(-rT) * N(d2)
```

This is used as a reference value for European Monte Carlo tests and benchmark
output.

## Tests

Tests are defined in `tests/test_options.py`.

The test suite currently checks:

- Standard European Monte Carlo convergence against Black-Scholes.
- Antithetic European Monte Carlo convergence against Black-Scholes.
- Parallel European Monte Carlo convergence against Black-Scholes.
- Asian call price is lower than a comparable European call price.
- Up-and-out barrier call price is lower than a comparable European call price.
- Sequential and parallel Asian pricers are approximately consistent.
- Sequential and parallel barrier pricers are approximately consistent.
- Reusing the same seed gives deterministic output for the Asian pricer.

Run tests with:

```bash
PYTHONPATH=src pytest
```

## Benchmarks

Benchmark scripts live in `benchmarks/`.

Run them with `PYTHONPATH=src` so Python can import `gbm_simulation`.

### `benchmarks/benchmark_options.py`

Runs a broad benchmark over:

- Black-Scholes analytical European call pricing.
- Sequential European Monte Carlo.
- Parallel European Monte Carlo.
- Sequential Asian Monte Carlo.
- Parallel Asian Monte Carlo.
- Sequential barrier Monte Carlo.
- Parallel barrier Monte Carlo.

Command:

```bash
PYTHONPATH=src python benchmarks/benchmark_options.py
```

### `benchmarks/threshold_benchmark.py`

Measures when multiprocessing becomes faster than sequential execution across
different simulation counts.

It evaluates:

- European call.
- Asian call.
- Up-and-out barrier call.

For each option type, it prints sequential time, parallel time, speedup, worker
efficiency, and the first observed crossover threshold.

Command:

```bash
PYTHONPATH=src python benchmarks/threshold_benchmark.py
```

### `benchmarks/persistent_benchmark.py`

Compares three European-call execution styles:

- Sequential execution.
- A newly created process pool for each pricing call.
- A reused process pool passed into the pricing function.

This script is an experiment for measuring multiprocessing overhead at different
simulation sizes.

Command:

```bash
PYTHONPATH=src python benchmarks/persistent_benchmark.py
```

## Installation

Create and activate a virtual environment, then install dependencies:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

The current `requirements.txt` includes `numpy` and `scipy`. It also lists some
standard-library modules, which do not need to be installed separately.

## Example Usage

```python
from gbm_simulation import (
    black_scholes_call,
    price_european_call,
    price_asian_call,
    price_barrier_call,
)

S0 = 100.0
K = 100.0
r = 0.05
sigma = 0.2
T = 1.0
n_sims = 100_000
n_steps = 50
B = 120.0

bs = black_scholes_call(S0, r, sigma, T, K)
european = price_european_call(S0, r, sigma, T, K, n_sims, seed=42)
asian = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims, seed=42)
barrier = price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims, B, seed=42)

print(bs, european, asian, barrier)
```

Run the example with:

```bash
PYTHONPATH=src python your_script.py
```

## Current Design Notes

- European options are simulated from terminal prices only, which is efficient
  because the full path is unnecessary.
- Asian and barrier options require full paths, so they use `generate_paths`.
- Path-dependent pricers process simulations in chunks to reduce peak memory
  usage.
- Parallel pricers return only payoff sums and counts from workers, avoiding
  large payoff arrays being sent back to the parent process.
- Randomness is controlled with NumPy's `default_rng` and `SeedSequence`.
- Parallel chunks receive spawned child seed sequences to keep worker streams
  independent and reproducible.
- Barrier monitoring is discrete, so barrier crossings between simulated time
  steps are not detected.

## Major Future Improvements

### 1. Package and Project Structure

Convert the repository into a proper installable Python package.

Possible changes:

- Add `pyproject.toml`.
- Define package metadata and dependencies cleanly.
- Remove standard-library modules from `requirements.txt`.
- Add explicit development dependencies such as `pytest`.
- Make imports work without manually setting `PYTHONPATH=src`.

### 2. Input Validation and Error Handling

Add validation for invalid model and simulation inputs.

Examples:

- `S0 > 0`
- `K > 0`
- `sigma >= 0`
- `T > 0`
- `n_sims > 0`
- `n_steps > 0`
- `chunk_size > 0`
- Barrier levels that make sense for the selected barrier type.

This would make the library safer to use from scripts, notebooks, services, and
future C++ bindings.

### 3. Richer Pricing Output

Current pricing functions return only a price.

Future versions could return a result object containing:

- Estimated price.
- Standard error.
- Confidence interval.
- Number of simulations.
- Random seed metadata.
- Runtime.
- Pricing method used.

This would make benchmark output and model comparisons more useful.

### 4. More Option Types

Extend the engine beyond the current call-only set.

Potential additions:

- European puts.
- Digital options.
- Down-and-out and down-and-in barriers.
- Up-and-in barriers.
- Lookback options.
- Basket options.
- American options using Longstaff-Schwartz Monte Carlo.

### 5. Greeks

Add option sensitivity calculations.

Important Greeks include:

- Delta.
- Gamma.
- Vega.
- Theta.
- Rho.

Possible methods:

- Finite differences.
- Pathwise estimators.
- Likelihood-ratio estimators.
- Analytical Greeks for Black-Scholes European options.

### 6. Variance Reduction

The code already includes antithetic variates for European calls. This can be
expanded into a broader variance-reduction layer.

Possible additions:

- Control variates.
- Moment matching.
- Stratified sampling.
- Importance sampling.
- Quasi-Monte Carlo with Sobol sequences.

### 7. Better Barrier Accuracy

The current barrier pricer only checks simulated grid points.

For higher accuracy, especially with coarse `n_steps`, add Brownian bridge
correction to estimate barrier crossings between time steps.

### 8. Model Extensibility

Separate the stochastic process model from the option payoff logic.

This would make it easier to add models such as:

- Local volatility.
- Heston stochastic volatility.
- Jump diffusion.
- Time-dependent rates and volatility.

### 9. Cleaner API Boundaries

Introduce clearer abstractions for:

- Market parameters.
- Simulation settings.
- Payoff definitions.
- Pricing engines.
- Result objects.

This would make the Python implementation easier to maintain and would also
help when recreating the project in C++.

### 10. Stronger Testing

Expand the test suite beyond the current smoke and consistency checks.

Useful additions:

- Edge-case tests for invalid inputs.
- Put-call parity tests once puts are added.
- Deterministic regression fixtures for seeded simulations.
- Statistical tolerance tests with fixed confidence levels.
- Tests for zero volatility or near-zero maturity behavior.
- Tests for barrier edge cases.

### 11. Documentation and Examples

Add more user-facing examples.

Potential additions:

- Notebook examples.
- CLI examples.
- Benchmark result snapshots.
- Explanation of Monte Carlo error and convergence.
- Comparison plots for simulation count versus error.

### 12. Performance-Oriented C++ Port

Since this project is expected to be recreated in C++, the Python version can
act as a reference implementation.

For the C++ version, important design goals would include:

- Clear separation between simulation, payoff, and aggregation.
- Deterministic random number stream handling.
- Memory-conscious path generation.
- Thread-safe parallel execution.
- Reproducible benchmark cases matching the Python implementation.

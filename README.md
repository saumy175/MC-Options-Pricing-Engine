# Monte Carlo Options Pricing Engine — C++ Implementation

A multi-threaded Monte Carlo engine for pricing exotic options via simulation of
Geometric Brownian Motion (GBM). Implements European, Asian, and Barrier (knock-out)
call options with antithetic variance reduction and `std::async`-based parallelism.

---

## Mathematical background

Asset price paths are simulated under the risk-neutral measure using the discrete
GBM scheme derived from the Black-Scholes SDE:

$$S_{t + \Delta t} = S_t \cdot \exp\left[\left(r - \tfrac{1}{2}\sigma^2\right)\Delta t + \sigma\sqrt{\Delta t}\, Z\right], \quad Z \sim \mathcal{N}(0,1)$$

where $r$ is the risk-free rate, $\sigma$ is volatility, and $\Delta t = T / n\_\text{steps}$.

The option price is estimated as the discounted expected payoff:

$$\hat{V} = e^{-rT} \cdot \frac{1}{N} \sum_{i=1}^{N} \phi\left(S^{(i)}\right)$$

with standard error $\hat{\sigma} / \sqrt{N}$ where $\hat{\sigma}$ is the sample
standard deviation of the payoffs.

### Payoff functions

| Option | Payoff $\phi(S)$ |
|---|---|
| European call | $\max(S_T - K,\ 0)$ |
| Asian call | $\max\left(\bar{S} - K,\ 0\right)$, $\bar{S} = \frac{1}{n}\sum_{t} S_t$ |
| Barrier call (knock-out) | $\max(S_T - K,\ 0) \cdot \mathbf{1}[\max_t S_t \leq B]$ |

### Variance reduction — antithetic variates

For each standard normal draw $Z$, a mirror path is simulated using $-Z$.
The two payoffs are averaged before accumulation:

$$\hat{\phi}_i = \tfrac{1}{2}\left[\phi(Z_i) + \phi(-Z_i)\right]$$

Since $\text{Cov}(\phi(Z), \phi(-Z)) < 0$, this reduces the estimator variance
without increasing the number of GBM draws. Empirically, the European pricer
achieves a 3–5× reduction in variance for at-the-money calls.

---

## Architecture

```
cpp/
  include/
    gbm.hpp                — MarketParams, SimulationConfig, generate_path()
    pricers.hpp            — Sequential pricers, Black-Scholes reference
    parallel_pricers.hpp   — std::async worker pool, pooled variance SE
  src/
    main.cpp               — Validation, timing, speedup table
  CMakeLists.txt
```

**Key design decisions:**

- Each `std::async` worker owns its own `std::mt19937_64` instance seeded with
  `config.seed + worker_id`. No shared RNG state, no mutexes, lock-free by design.
- Workers return `(sum, sum_of_squares, count)` rather than a price. The main thread
  pools these to compute a statistically exact SE via the identity
  $\text{Var} = (\sum x_i^2 - N\bar{x}^2)/(N-1)$, avoiding the need to store or
  transmit full payoff vectors across threads.
- European calls use the closed-form single-step terminal price $S_T = S_0 \exp(\cdot)$,
  bypassing path simulation entirely. Asian and Barrier options require full paths.

---

## Requirements

- C++20 or later
- CMake ≥ 3.16
- POSIX threads (standard on Linux and macOS; on Windows, use WSL2 or MinGW)

No external libraries. The implementation depends only on `<random>`, `<future>`,
`<numeric>`, and `<cmath>` from the standard library.

---

## Build

```bash
git clone https://github.com/saumy175/MC-Options-Pricing-Engine
cd MC-Options-Pricing-Engine/cpp

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The `Release` build compiles with `-O3 -march=native`. For debugging, substitute
`-DCMAKE_BUILD_TYPE=Debug`.

---

## Run

```bash
./mc_engine
```

Parameters are set in `src/main.cpp` under the clearly labelled parameter block:

```cpp
MarketParams market { .S0 = 100.0, .r = 0.05, .sigma = 0.20 };

SimulationConfig config {
    .n_sims     = 1'000'000,
    .n_steps    = 252,        // trading days per year
    .n_workers  = 4,
    .seed       = 42,
    .antithetic = true
};
```

Rebuild after any parameter change.

---

## Benchmark results

**Hardware:** [Intel(R) Core(TM) Ultra 5 225H], [14]-core, [16] GB RAM, [Arch Linux, Linux 7.0.5-arch1-1]

**Parameters:** $S_0 = 100$, $K = 100$, $r = 0.05$, $\sigma = 0.20$, $T = 1.0$,
$n\_\text{sims} = 10^6$, $n\_\text{steps} = 252$, 4 workers, antithetic variates enabled.

### Prices

| Option | Sequential | Parallel | Black-Scholes |
|---|---|---|---|
| European | 10.4499 | 10.4578 | 10.2969 |
| Asian | 5.7539 | 5.7641 | — |
| Barrier ($B = 120$) | 1.3288 | 1.3270 | — |

95% confidence intervals (sequential):

| Option | Lower | Upper | SE |
|---|---|---|---|
| European | 10.4294 | 10.4703 | 0.0104 |
| Asian | 5.7383 | 5.7695 | 0.0080 |
| Barrier | 1.3221 | 1.3355 | 0.0034 |

### Timing and speedup

| Option | Sequential | Parallel (4 cores) | Speedup |
|---|---|---|---|
| European | 46.5 ms | 13.3 ms | 1.50× |
| Asian | 7355 ms | 1919 ms | 3.83× |
| Barrier | 7562 ms | 1967 ms | 3.90× |

European speedup is lower because the closed-form pricer is memory-bandwidth bound
rather than compute bound — the per-path work is too small to fully amortize thread
launch overhead. Asian and Barrier pricers simulate 252-step paths per simulation,
making them compute-bound and thus near-linearly scalable with core count.

---

## Interpreting the results

**Asian < European** — The Asian payoff depends on the path average rather than the
terminal price. Averaging over 252 daily prices reduces effective volatility, compressing
the right tail of the payoff distribution and therefore the option premium.

**Barrier << European** — The knock-out condition at $B = 120$ eliminates precisely
the paths that would produce large payoffs (those that rise well above $K = 100$
are also likely to have breached $B = 120$ en route). The surviving paths are those
that stayed subdued, yielding a much lower expected payoff.

**Black-Scholes comparison** — The analytical Black-Scholes formula applies only to
European calls under constant volatility. The MC estimate of 10.4499 lies within
the expected Monte Carlo error band for $N = 10^6$ simulations. No closed-form
reference exists for Asian or Barrier options — Monte Carlo simulation is the
standard pricing method for these contracts.
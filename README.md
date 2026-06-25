# Monte Carlo Options Pricing Engine — C++ Implementation

A multi-threaded Monte Carlo engine for pricing exotic options via simulation of
Geometric Brownian Motion (GBM). Implements European, Asian, and Barrier (knock-out)
call options with antithetic variance reduction and `std::async`-based parallelism.

---

## Mathematical background

Asset price paths are simulated under the risk-neutral measure using the discrete
GBM scheme derived from the Black-Scholes SDE:

$$S_{t+\Delta t}=S_t\exp((r-\frac{1}{2}\sigma^2)\Delta t+\sigma\sqrt{\Delta t}Z),  Z\sim N(0,1)$$
where $r$ is the risk-free rate, $\sigma$ is volatility, and $\Delta t = T / n_{\text{steps}}$.

The option price is estimated as the discounted expected payoff:

$$
\hat{V}
= e^{-rT} \cdot \frac{1}{N} \sum_{i=1}^{N} \phi\left(S^{(i)}\right)
$$

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

$$
\hat{\phi}_i
= \tfrac{1}{2}\left[\phi(Z_i) + \phi(-Z_i)\right]
$$

Since $\text{Cov}(\phi(Z), \phi(-Z)) < 0$, this reduces the estimator variance
without increasing the number of GBM draws. Empirically, the European pricer
achieves a 3–5× reduction in variance for at-the-money calls.

---

## Architecture

```text
benchmarks/
  pricing.hpp            — pricing benchmark: sequential vs parallel prices, CI, timing
  scalability.hpp        — worker-scaling benchmark: speedup vs number of workers
  convergence.hpp        — Monte Carlo convergence benchmark: error vs number of paths

include/
  cli.hpp                — command-line parsing and default parameter block
  greeks.hpp             — finite-difference Greeks
  gbm.hpp                — GBM path generation under the risk-neutral measure
  parallel_pricers.hpp   — parallel Monte Carlo pricers and worker aggregation
  pricers.hpp       — sequential pricers and analytical Black-Scholes reference
```


## Run

This is a CLI-based program.

```bash
Usage: ./mc_engine [options]

Market parameters:
  --S0      <float>   Starting price         (default: 100.0)
  --r       <float>   Risk-free rate         (default: 0.05)
  --sigma   <float>   Volatility             (default: 0.20)

Option parameters:
  --K       <float>   Strike price           (default: 100.0)
  --T       <float>   Time to expiry (years) (default: 1.0)
  --B       <float>   Barrier level          (default: 120.0)

Simulation parameters:
  --sims    <int>     Number of simulations  (default: 1000000)
  --steps   <int>     Path steps per sim     (default: 252)
  --workers <int>     Parallel workers       (default: 4)
  --seed    <int>     RNG seed               (default: 42)
  --no-antithetic     Disable antithetic variates

Mode:
  --mode    <string>  pricing | scalability | convergence | greeks | all
                      (default: all)

Examples:
  ./mc_engine
  ./mc_engine --mode pricing --S0 110 --K 105 --sims 500000
  ./mc_engine --mode greeks
  ./mc_engine --mode scalability --sims 2000000
```

To change the default values, edit `include/cli.hpp` and change them at `src/main.cpp` as well.

## Benchmark results

**Hardware:** `Intel(R) Core(TM) Ultra 5 225H`, `14-core CPU`, `16 GB RAM`, `Arch Linux`

**Benchmark parameters used in the output below:**  
$S_0 = 100$, $K = 100$, $r = 0.05$, $\sigma = 0.20$, $T = 1.0$,  
$B = 120$, $N = 10^6$, $n_{\text{steps}} = 252$, $n_{\text{workers}} = 4, antithetic variates enabled.

### What the benchmarks measure

| Benchmark | What it measures | Main quantity of interest |
|---|---|---|
| Pricing | Final option value, confidence interval, and runtime | Accuracy of the Monte Carlo estimator and end-to-end performance |
| Scalability (for Asian and Barrier Calls) | Runtime as workers increase | Parallel speedup and overhead amortization |
| Convergence | Error decay as sample size grows | $N^{-1/2}$ convergence law |

### 1) Pricing benchmark

| Contract | Sequential price | Parallel price | Black-Scholes |
|---|---|---|---|
| European call | 10.4499 | 10.4578 | 10.2969 |
| Asian call | 5.7539 | 5.7641 | — |
| Barrier call ($B=120$) | 1.3288 | 1.3270 | — |

Sequential 95% confidence intervals:

| Contract | Lower | Upper | SE |
|---|---|---|---|
| European call | 10.4294 | 10.4703 | 0.0104 |
| Asian call | 5.7383 | 5.7695 | 0.0080 |
| Barrier call | 1.3221 | 1.3355 | 0.0034 |

Timing:

| Contract | Sequential time | Parallel time (4 workers) | Speedup |
|---|---|---|---|
| European call | 42.4 ms | 12.2 ms | 3.48× |
| Asian call | 7318.5 ms | 1906.1 ms | 3.84× |
| Barrier call | 7604.5 ms | 1938.9 ms | 3.92× |

The pricing benchmark compares the Monte Carlo estimate

$$
\hat V_N = e^{-rT}\frac{1}{N}\sum_{i=1}^N X_i
$$

against a closed-form Black-Scholes price, and it also reports the empirical standard error

$$
\widehat{\mathrm{SE}} = \frac{s_N}{\sqrt{N}}, \qquad
s_N^2 = \frac{1}{N-1}\sum_{i=1}^N (X_i-\bar X)^2.
$$

For the European call, the Monte Carlo price should sit near the Black-Scholes value because both are pricing the same contract under the same risk-neutral GBM model. The small difference is probably sampling noise along with the usual Monte Carlo error.

### 2) Scalability benchmark

| Workers | Asian time (ms) | Speedup | Barrier time (ms) | Speedup |
|---|---|---|---|---|
| 1  | 7212.8 | 1.00× | 7402.3 | 1.00× |
| 2  | 3665.1 | 1.97× | 3812.2 | 1.94× |
| 4  | 1889.1 | 3.82× | 1956.8 | 3.78× |
| 8  | 1399.3 | 5.15× | 1423.3 | 5.20× |
| 16 | 1056.7 | 6.83× | 1031.4 | 7.18× |

The scalability benchmark measures how well the workload parallelizes. The ideal speedup with $p$ workers is

$$
S(p) = \frac{T(1)}{T(p)} \approx p,
$$

but in practice it is limited by serial overhead, worker startup, synchronization, and memory effects. A standard upper bound is Amdahl’s law:

$$
S(p) = \frac{1}{f + \frac{1-f}{p}},
$$

where $f$ is the serial fraction of the code. As $p$ grows, the parallel section dominates less of the total runtime improvement, so speedup bends away from the ideal straight line.

Asian and Barrier options scale much better than a tiny closed-form calculation because they do substantial per-simulation work: each path contains 252 GBM steps, repeated over a large number of simulations. That makes the workload compute-bound, so adding workers is effective until overhead starts to dominate. The observed speedups are therefore close to linear at low worker counts and then flatten gradually at higher counts.

### 3) Convergence benchmark

| N | Price | SE | abs(MC - BS) | SE ratio |
|---|---|---|---|---|
| 1,000 | 10.6350 | 0.3405 | 0.3381 | — |
| 5,000 | 10.6020 | 0.1498 | 0.3051 | 2.27× |
| 10,000 | 10.5167 | 0.1041 | 0.2198 | 1.44× |
| 50,000 | 10.4829 | 0.0464 | 0.1860 | 2.24× |
| 100,000 | 10.4520 | 0.0328 | 0.1551 | 1.41× |
| 500,000 | 10.4590 | 0.0148 | 0.1621 | 2.22× |
| 1,000,000 | 10.4499 | 0.0104 | 0.1529 | 1.42× |

The convergence benchmark checks the core Monte Carlo law:

$$
\mathrm{Var}(\hat V_N) = \frac{\mathrm{Var}(X)}{N},
\qquad
\mathrm{SE}(\hat V_N) \propto \frac{1}{\sqrt{N}}.
$$

So when the sample size $N$ grows by a factor of $c$, the standard error should shrink by about $\sqrt{c}$, which is evidently seen here: going from 1,000 to 1,000,000 paths reduces the standard error from about 0.3405 to 0.0104, which is roughly a factor of 33 reduction close to the theoretical $\sqrt{1000} \approx 31.6$.

The $|MC - BS|$ column should also contract with larger $N$, but not monotonically every time because each estimate is still random. The important point is that the error band tightens as $N^{-1/2}$, so the estimate becomes more stable and the confidence interval narrows.

---

## Interpreting the results

**European vs Black-Scholes** — The European call is the only contract here with a closed-form Black-Scholes benchmark. Under the same GBM dynamics used by the simulator, the Monte Carlo estimator is unbiased:

$$
\mathbb{E}[\hat V_N] = V.
$$

So the simulated price should fluctuate around the analytical price, with typical deviation on the order of one standard error.

**Asian < European** — The Asian payoff depends on the arithmetic average

$$
\bar S = \frac{1}{n}\sum_{j=1}^{n} S_{t_j}
$$

instead of only the terminal price $S_T$. Averaging reduces path-to-path variability, so the right tail is less explosive than in a plain European call. Since the call payoff is convex but the averaging smooths the underlying path, the expected payoff is lower than the European call for the same $S_0, K, r, \sigma, T$.

**Barrier << European** — The knock-out barrier removes paths that ever cross $B$:

$$
X = e^{-rT}(S_T-K)^+\mathbf{1}\left[\max_{0\le t\le T} S_t < B\right].
$$

This indicator kills many of the paths that would otherwise contribute large payoffs. In other words, the barrier truncates the high-payoff tail, so the price collapses relative to the European contract.

**Runtime Trends** — European pricing is cheap because it only needs the terminal GBM draw, so parallelization helps less. Asian and Barrier pricing are expensive because each path requires repeated time stepping:

$$
S_{t_{j+1}} = S_{t_j}\exp\left((r-\tfrac12\sigma^2)\Delta t + \sigma\sqrt{\Delta t}\Z_j\right),
$$

which makes the CPU do much more work per simulation. That extra arithmetic is what allows parallel workers to pay off. In short: more per-path work means better scaling.
# Options Pricing Engine

This repository is being reset around a new dataclass-based Python API before
adding more quantitative features or porting the engine to C++.

The current code intentionally keeps only three Monte Carlo pricing functions:

- `price_european_call`
- `price_asian_call`
- `price_barrier_call`

There are currently no tests or benchmark scripts. Those will be rebuilt around
the new API.

## Structure

```text
options_pricing_engine/
+-- src/
|   +-- datatypes.py
|   +-- gbm_simulation.py
+-- requirements.txt
+-- README.md
```

## Data Types

`src/datatypes.py` defines the API objects passed into and returned from the
pricing functions.

```python
MarketParams(S0, r, sigma)
EuropeanOptionParams(K, T)
AsianOptionParams(K, T)
BarrierOptionParams(K, T, B)
SimulationConfig(n_sims, n_steps=None, seed=None, chunk_size=50000, n_workers=4, antithetic=True)
PricingResult(price, standard_error, confidence_interval, n_sims, method, seed)
```

## Pricing Functions

`src/gbm_simulation.py` exposes only:

```python
price_european_call(market, option, config)
price_asian_call(market, option, config)
price_barrier_call(market, option, config)
```

Each function returns a `PricingResult` containing:

- `price`
- `standard_error`
- `confidence_interval`
- `n_sims`
- `method`
- `seed`

## Example

```python
from datatypes import (
    AsianOptionParams,
    BarrierOptionParams,
    EuropeanOptionParams,
    MarketParams,
    SimulationConfig,
)
from gbm_simulation import (
    price_asian_call,
    price_barrier_call,
    price_european_call,
)

market = MarketParams(S0=100.0, r=0.05, sigma=0.2)
config = SimulationConfig(n_sims=100_000, n_steps=50, seed=42)

european = price_european_call(
    market,
    EuropeanOptionParams(K=100.0, T=1.0),
    config,
)

asian = price_asian_call(
    market,
    AsianOptionParams(K=100.0, T=1.0),
    config,
)

barrier = price_barrier_call(
    market,
    BarrierOptionParams(K=100.0, T=1.0, B=120.0),
    config,
)

print(european)
print(asian)
print(barrier)
```

Run scripts with:

```bash
PYTHONPATH=src python your_script.py
```

## Current Limitations

- No input validation yet.
- No tests yet.
- No benchmarks yet.
- No Black-Scholes analytical reference function.
- No parallel pricing functions.
- Asian and barrier pricing allocate full path matrices in memory.
- Barrier monitoring checks only discrete simulated time steps.


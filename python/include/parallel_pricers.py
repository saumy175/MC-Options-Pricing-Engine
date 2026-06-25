#INCOMPLETE PARALLEL IMPLEMENTATION, currently working around Python's GIL

import numpy as np
import python.include.datatypes as datatypes
from dataclasses import dataclass

@dataclass(frozen=True)
class BatchResult:
    payoff_sum: float
    payoff_sum_sq: float
    count: int

def european_worker(
    market: datatypes.MarketParams,
    option: datatypes.EuropeanOptionParams,
    config: datatypes.SimulationConfig
) -> BatchResult:
    
    rng = np.random.Generator(np.random.MT19937(config.seed))
    S0, r, sigma = market.S0, market.r, market.sigma
    K, T = option.K, option.T

    drift = (r - 0.5 * sigma**2) * T
    vol = sigma * np.sqrt(T)

    sum = 0.0
    sum_squared = 0.0
    cnt = 0

    if config.antithetic:
        for _ in range(config.n_sims/2):
            Z = rng.normal(0.0, 1.0)
            ST1 = S0*np.exp(drift + vol*Z)
            ST2 = S0*np.exp(drift - vol*Z)

            p1 = np.maximum(ST1 - K, 0)
            p2 = np.maximum(ST2 - K, 0)
            p = 0.5*(p1 + p2)
            sum += p
            sum_squared += p*p
            cnt+=1
            
    else:
        for _ in range(config.n_sims):
            Z = rng.normal(0.0, 1.0)
            ST = S0*np.exp(drift + vol*Z)
            p = np.maximum(ST - K, 0)
            sum += p
            sum_squared += p*p
            cnt+=1
    
    return BatchResult(sum, sum_squared, cnt)
            
def price_european_call_parallel(
    market: datatypes.MarketParams,
    option: datatypes.EuropeanOptionParams,
    config: datatypes.SimulationConfig
) -> datatypes.PricingResult:
    
    n_workers = config.n_workers
    n_sims_each = config.n_sims / n_workers
    remainder_sims = config.n_sims % n_workers

    


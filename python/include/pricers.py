import numpy as np
import python.include.datatypes as datatypes
import python.include.helper as helper

def price_european_call(
    market: datatypes.MarketParams,
    option: datatypes.EuropeanOptionParams,
    config: datatypes.SimulationConfig,
) -> datatypes.PricingResult:
    rng = np.random.Generator(np.random.MT19937(config.seed))
    S0, r, sigma = market.S0, market.r, market.sigma
    K, T = option.K, option.T

    drift = (r - 0.5 * sigma**2) * T
    vol = sigma * np.sqrt(T)

    payoffs = []
    if config.antithetic:
        for _ in range(config.n_sims/2):
            Z = rng.normal(0.0, 1.0)
            ST1 = S0*np.exp(drift + vol*Z)
            ST2 = S0*np.exp(drift - vol*Z)

            p1 = np.maximum(ST1 - K, 0)
            p2 = np.maximum(ST2 - K, 0)
            payoffs.append(0.5*(p1 + p2))
    else:
        for _ in range(config.n_sims):
            Z = rng.normal(0.0, 1.0)
            ST = S0*np.exp(drift + vol*Z)
            p = np.maximum(ST - K, 0)
            payoffs.append(p)

    return helper.make_result(payoffs, r, T, config.n_sims, "price_european_call")


def price_asian_call(
    market: datatypes.MarketParams,
    option: datatypes.AsianOptionParams,
    config: datatypes.SimulationConfig,
) -> datatypes.PricingResult:
    
    rng = np.random.Generator(np.random.MT19937(config.seed))
    
    K = option.K
    T = option.T

    payoffs = []

    for _ in range(config.n_sims):
        path = helper.generate_path(market, T, config.n_steps, rng)
        mean = np.mean(path)
        payoffs.append(np.maximum(mean - K), 0)
    
    return helper.make_result(payoffs, market.r, T, config.n_sims, "price_asian_call")

def price_barrier_call(
    market: datatypes.MarketParams,
    option: datatypes.BarrierOptionParams,
    config: datatypes.SimulationConfig,
) -> datatypes.PricingResult:
    
    rng = np.random.Generator(np.random.MT19937(config.seed))
    
    K = option.K
    T = option.T
    B = option.B

    payoffs = []

    for _ in range(config.n_sims):
        path = helper.generate_path(market, T, config.n_steps, rng)
        M = np.max(path)
        p = 0
        if (M > B): p = 0
        else: p = np.maximum(path[-1] - K, 0)
        payoffs.append(p)
    
    return helper.make_result(payoffs, market.r, T, config.n_sims, "price_barrier_call")

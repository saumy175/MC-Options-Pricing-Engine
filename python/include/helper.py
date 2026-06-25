import numpy as np
import python.include.datatypes as datatypes
import string

def generate_path(
    market: datatypes.MarketParams,
    T: float,
    n_steps: int,
    rng: np.random.Generator
) -> np.ndarray:
    dt = T/n_steps
    drift = (market.r - 0.5*market.sigma*market.sigma)*dt
    vol = market.sigma*np.sqrt(dt)

    path = np.empty(n_steps+1)
    path[0] = market.S0
    for i in range(1, n_steps):
        Z = rng.normal(0.0, 1.0)
        path[i] = path[i-1]*np.exp(drift + vol*Z)

    return path

def mean_and_se(
    path: np.ndarray
) -> tuple:
    n = np.size(path)
    mean = np.mean(path)

    sq = 0
    for x in path:
        sq += (x - mean)*(x - mean)
    variance = sq/(n-1)
    se = np.sqrt(variance/n)

    return (mean, se)

def make_result(
    payoffs: np.ndarray,
    r: float,
    T: float,
    n_sims: int,
    method: string
) -> datatypes.PricingResult:
    mean, se = mean_and_se(payoffs)
    D = np.exp(-r*T)
    price = D*mean
    standard_error = D*se

    return datatypes.PricingResult(price, standard_error, (price - 1.96*standard_error, price + 1.96*standard_error), n_sims, method)

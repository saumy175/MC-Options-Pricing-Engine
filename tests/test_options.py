import numpy as np
import pytest

from gbm_simulation import (
    price_european_call,
    price_european_call_antithetic,
    price_european_call_parallel,
    black_scholes_call,
    price_asian_call,
    price_asian_call_parallel,
    price_barrier_call,
    price_barrier_call_parallel
)

# --- Global Test Parameters ---
S0 = 100.0
K = 100.0
r = 0.05
sigma = 0.2
T = 1.0
B = 120.0  # Barrier
n_steps = 50
n_sims_euro = 500_000    # High sims for BS convergence
n_sims_path = 50_000     # Lower sims for path-dependent to keep tests fast
TOLERANCE = 0.05         # Acceptable absolute error for MC vs Analytical

@pytest.fixture
def bs_price():
    """Provides the exact Black-Scholes price for the standard parameters."""
    return black_scholes_call(S0, r, sigma, T, K)

def test_european_mc_vs_black_scholes(bs_price):
    """Test standard European MC against exact Black-Scholes."""
    mc_price = price_european_call(S0, r, sigma, T, K, n_sims=n_sims_euro, seed=42)
    assert mc_price == pytest.approx(bs_price, abs=TOLERANCE)

def test_european_antithetic_vs_black_scholes(bs_price):
    """Test antithetic European MC against exact Black-Scholes."""
    mc_price = price_european_call_antithetic(S0, r, sigma, T, K, n_sims=n_sims_euro, seed=42)
    assert mc_price == pytest.approx(bs_price, abs=TOLERANCE)

def test_european_parallel_vs_black_scholes(bs_price):
    """Test parallel European MC against exact Black-Scholes."""
    mc_price = price_european_call_parallel(S0, r, sigma, T, K, n_sims=n_sims_euro, seed=42)
    assert mc_price == pytest.approx(bs_price, abs=TOLERANCE)

def test_asian_vs_european():
    """An Asian option should generally be cheaper than a standard European option 
    because averaging reduces volatility."""
    euro_price = price_european_call(S0, r, sigma, T, K, n_sims_path, seed=42)
    asian_price = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims_path, seed=42)
    assert asian_price < euro_price

def test_barrier_vs_european():
    """An Up-and-Out Barrier option must be strictly cheaper than a standard European option."""
    euro_price = price_european_call(S0, r, sigma, T, K, n_sims_path, seed=42)
    barrier_price = price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims_path, B, seed=42)
    assert barrier_price < euro_price

def test_parallel_vs_sequential_asian():
    """Ensure parallel and sequential Asian pricers yield roughly the same result."""
    seq_price = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims_path, seed=1)
    par_price = price_asian_call_parallel(S0, r, sigma, T, K, n_steps, n_sims_path, seed=1)
    # Using a slightly wider tolerance because seed spawning might create different RNG streams
    assert par_price == pytest.approx(seq_price, rel=0.01)

def test_parallel_vs_sequential_barrier():
    """Ensure parallel and sequential Barrier pricers yield roughly the same result."""
    seq_price = price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims_path, B, seed=1)
    par_price = price_barrier_call_parallel(S0, r, sigma, T, K, B, n_steps, n_sims_path, seed=1)
    assert par_price == pytest.approx(seq_price, rel=0.01)

def test_seed_determinism():
    """Running the exact same simulation twice with the same seed should yield identical results."""
    price1 = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims=10000, seed=99)
    price2 = price_asian_call(S0, r, sigma, T, K, n_steps, n_sims=10000, seed=99)
    assert price1 == price2
import numpy as np
from scipy.stats import norm

from multiprocessing import Pool

#create chunk_sizes (name suggests it)
def chunk_sizes(total, chunk_size):
    chunks = []
    while total > 0:
        current = min(chunk_size, total)
        chunks.append(current)
        total -= current
    return chunks

#this function is pretty useless, just here cause why not, was generate_path() before, now changed to _generate_path
def _generate_path(S0, r, sigma, T, n_steps, seed = None):
    '''
    Returns a 1D array P = [S0, S_dt, S_2dt ..., S_(n_steps*dt)=S_T] which is of size n_steps+1
    '''
    rng = np.random.default_rng(seed=seed)
    dt = T / n_steps

    drift = (r - sigma**2 /2)*dt
    vol = sigma * np.sqrt(dt)

    Z = rng.standard_normal(n_steps)

    log_returns = drift + vol*Z
    log_prices = np.concatenate([[np.log(S0)], np.log(S0) + np.cumsum(log_returns)])

    return np.exp(log_prices)

def generate_paths(S0, r, sigma, T, n_steps, n_sims, seed=None):
    '''
    Returns of 2D array P of size n_sims x (n_steps+1) where each row is the path of size n_steps+1 with GBM simulation similar to generate_path
    '''
    rng = np.random.default_rng(seed)
    dt = T / n_steps
    drift = (r - 0.5 * sigma**2) * dt
    vol = sigma * np.sqrt(dt)
    logS0 = np.log(S0)
    Z = rng.standard_normal((n_sims, n_steps))
    log_returns = drift + vol * Z

    log_prices = np.empty((n_sims, n_steps + 1))
    log_prices[:, 0] = logS0
    log_prices[:, 1:] = logS0 + np.cumsum(log_returns, axis=1)

    return np.exp(log_prices)

def worker_european_call(args):
    '''
    Simulate one chunk of European call payoffs.
    Returns (sum_of_payoffs, number_of_payoffs).
    '''
    S0, r, sigma, T, K, n_sims, seed, antithetic = args

    rng = np.random.default_rng(seed)
    drift = (r - 0.5 * sigma**2) * T
    vol = sigma * np.sqrt(T)

    if antithetic:
        m = n_sims // 2 

        Z = rng.standard_normal(m)
        ST1 = S0 * np.exp(drift + vol * Z)
        ST2 = S0 * np.exp(drift - vol * Z)

        payoff1 = np.maximum(ST1 - K, 0.0)
        payoff2 = np.maximum(ST2 - K, 0.0)

        payoffs = 0.5 * (payoff1 + payoff2)
    else:
        Z = rng.standard_normal(n_sims)
        ST = S0 * np.exp(drift + vol * Z)
        payoffs = np.maximum(ST - K, 0.0)

    return payoffs.sum(), payoffs.size

def price_european_call_parallel(S0, r, sigma, T, K, n_sims, n_workers=4, chunk_size=200_000, seed=1, antithetic=True):
    '''
    Parallel European call pricer with chunking.
    chunk_size: number of paths per task sent to a worker
    '''
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
'''
The following are definition of common parameters used
S0: starting price
r: risk free rate
sigma: volatility
T: time horizon in years
K: strike price
n_sims: number of stimulated paths
'''

def price_european_call(S0, r, sigma, T, K, n_sims, seed=1):

    rng = np.random.default_rng(seed)
    Z = rng.standard_normal(n_sims)
    drift = (r - sigma**2 /2)*T
    ST = S0*np.exp(drift + sigma*np.sqrt(T)*Z)

    payoffs = np.maximum(ST-K, 0)
    price = np.exp(-r*T)*np.mean(payoffs)

    return price

#this is also pretty useless because price_european_call_parallel has antithetic: Bool as a parameter, but this is just here for variation reduction demonstration, which is also not used 
def price_european_call_antithetic(S0, r, sigma, T, K, n_sims, seed=1):

    # antithetic is to have simulation of payoffs with both Z and -Z and average them
    rng = np.random.default_rng(seed)
    drift = (r - sigma**2 /2)* T

    Z = rng.standard_normal(n_sims//2)
    ST1 = S0*np.exp(drift + sigma*np.sqrt(T)*Z)
    ST2 = S0*np.exp(drift + sigma*np.sqrt(T)*(-Z))

    payoff1 = np.maximum(ST1-K, 0)
    payoff2 = np.maximum(ST2-K, 0)

    payoffs = (payoff1+payoff2)/2

    price = np.exp(-r*T)*np.mean(payoffs)
    return price

def worker_asian_call(args):
    S0, r, sigma, T, K, n_steps, n_sims, seed = args
    paths = generate_paths(S0, r, sigma, T, n_steps, n_sims, seed)
    avg_price = paths.mean(axis=1)
    payoffs = np.maximum(avg_price - K, 0)
    return payoffs.sum(), payoffs.size

def price_asian_call_parallel(S0, r, sigma, T, K, n_steps, n_sims, n_workers=4, chunk_size=200_000, seed=1):
    chunks = chunk_sizes(n_sims, chunk_size)
    seed_seq = np.random.SeedSequence(seed)
    child_seqs = seed_seq.spawn(len(chunks))

    args_list = [(S0, r, sigma, T, K, n_steps, chunks[i], child_seqs[i]) for i in range(len(chunks))]

    with Pool(processes=n_workers) as pool:
        results = pool.map(worker_asian_call, args_list)
    
    total_payoff = sum(s for s, _ in results)
    total_count = sum(c for _, c in results)

    return np.exp(-r*T)*(total_payoff/total_count) 

def price_asian_call(S0, r, sigma, T, K, n_steps, n_sims, seed=None, chunk_size=50000):
    seed_seq = np.random.SeedSequence(seed)
    chunks = chunk_sizes(n_sims, chunk_size)

    child_seeds = seed_seq.spawn(len(chunks))

    payoff_sum = 0
    payoff_count = 0

    for sims, child_seed in zip(chunks, child_seeds):
        paths = generate_paths(S0, r, sigma, T, n_steps, sims, seed=child_seed)
        avg_prices = paths.mean(axis=1)

        payoffs = np.maximum(avg_prices - K, 0)

        payoff_sum += payoffs.sum()
        payoff_count += payoffs.size
    
    return np.exp(-r*T)*(payoff_sum/payoff_count)

def worker_barrier_call(args):
    S0, r, sigma, T, K, n_steps, B, n_sims, seed = args
    paths = generate_paths(S0, r, sigma, T, n_steps, n_sims, seed)

    knocked_out = paths.max(axis=1) > B
    ST = paths[:, -1]
    payoffs = np.where(knocked_out, 0.0, np.maximum(ST - K, 0.0))

    return payoffs.sum(), payoffs.size

def price_barrier_call_parallel(S0, r, sigma, T, K, B, n_steps, n_sims, n_workers=4, chunk_size=50_000,seed=1):

    chunks = chunk_sizes(n_sims, chunk_size)
    seed_seq = np.random.SeedSequence(seed)
    child_seqs = seed_seq.spawn(len(chunks))

    args_list = [(S0, r, sigma, T, K, n_steps, B, chunks[i], child_seqs[i]) for i in range(len(chunks))]

    with Pool(processes=n_workers) as pool:
        results = pool.map(worker_barrier_call, args_list)

    total_payoff = sum(s for s, _ in results)
    total_count = sum(c for _, c in results)

    return np.exp(-r*T) * (total_payoff / total_count)

def price_barrier_call(S0, r, sigma, T, K, n_steps, n_sims, B, seed=None, chunk_size=50000):
    '''
    B: barrier
    '''
    seed_seq = np.random.SeedSequence(seed)
    chunks = chunk_sizes(n_sims, chunk_size)
    child_seeds = seed_seq.spawn(len(chunks))

    payoff_sum = 0
    payoff_count = 0

    for sims, child_seed in zip(chunks, child_seeds):
        paths = generate_paths(S0, r, sigma, T, n_steps, sims, seed=child_seed)

        knocked_out = (paths.max(axis=1) > B)
        ST = paths[:, -1]
        payoffs = np.where(knocked_out, 0.0, np.maximum(ST-K, 0))

        payoff_sum += payoffs.sum()
        payoff_count += payoffs.size

    return np.exp(-r * T) * (payoff_sum/payoff_count)
    
def black_scholes_call(S0, r, sigma, T, K):
    d1 = (np.log(S0/K) + (r+sigma**2 /2)*T)/(sigma*np.sqrt(T))
    d2 = d1 - sigma*np.sqrt(T)

    return S0*norm.cdf(d1) - K*np.exp(-r*T)*norm.cdf(d2)



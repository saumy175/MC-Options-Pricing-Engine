#pragma once

#include <string> // for PricingResult.method
#include <vector> // for generate_path
#include <random> // for normal distribution
#include <cmath> // for math (used here in exp and sqrt)

struct MarketParams {
    double S0;
    double r;
    double sigma;
};

struct EuropeanOptionsParams {
    double K;
    double T;
};

struct AsianOptionParams {
    double K;
    double T;
};

struct BarrierOptionParams {
    double K;
    double T;
    double B;
};

struct SimulationConfig {
    int n_sims;
    int n_steps = 252;
    int seed;
    int n_workers = 4;
    bool antithetic = true;
};

struct PricingResult {
    double price;
    double standard_error;
    std::pair<double, double> ci;
    int n_sims;
    std::string method;
};

/*
GBM path generator

Returns a vector of (n_steps + 1) prices: [S0, S_dt, S_2dt, ..., S_T]

The discrete GBM step is:
S(t+dt) = S(t) * exp(drift + vol*Z), where
drift = (r - sigma**2 /2)*sqrt(dt)
vol = sigma*sqrt(dt)
Z~N(0,1) is drawn at every step.
Note: rng is passed by reference so the caller controls seeding — critical for reproducibility and for giving each thread its own independent RNG.
*/

inline std::vector<double> generate_path(
    const MarketParams &market,
    double T,
    int n_steps,
    std::mt19937_64 &rng
) {
    const double dt = T / n_steps;
    const double drift = (market.r - 0.5*market.sigma*market.sigma)*dt;
    const double vol = market.sigma*std::sqrt(dt);
    std::normal_distribution<double> normal(0.0, 1.0);

    std::vector<double> path(n_steps+1);
    path[0] = market.S0;

    for (int i = 1; i <= n_steps; i++) {
        double Z = normal(rng);
        path[i] = path[i-1]*std::exp(drift + vol*Z);
    }

    return path;
}
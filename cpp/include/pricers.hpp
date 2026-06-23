#pragma once

#include "gbm.hpp"
#include <vector>
#include <cmath>
#include <string>
/*
Helper functions:
mean_and_se: evaluates mean and standard_error = standard_deviation/sqrt(n)
make_result: makes a PricingResult from payoffs
*/

namespace helper{

inline std::pair<double, double> mean_and_se(const std::vector<double> &path) {
    double n = static_cast<double>(path.size());
    double mean = std::accumulate(path.begin(), path.end(), 0.0) / n;

    double sq = 0.0;

    for (auto &x: path) sq += (x - mean)*(x- mean);
    double variance = sq/(n-1.0); // Sample Variance (Population variance is sq/n)
    double se = std::sqrt(variance/n);

    return {mean, se};
}

inline PricingResult make_result(
    const std::vector<double> &payoffs,
    double r, double T,
    int n_sims,
    const std::string &method
) {
    auto [mean, se] = mean_and_se(payoffs);
    double D = std::exp(-r*T);
    double price = D * mean;
    double standard_error = D*se;

    return PricingResult{price, standard_error, {price - 1.96*standard_error, price + 1.96*standard_error}, n_sims, method};
}

} //namespace helper

/*
Black-Scholes Model
refer: https://en.wikipedia.org/wiki/Black%E2%80%93Scholes_model

*/
inline double black_scholes_call(
    const MarketParams &market,
    const EuropeanOptionsParams &option
) {
    auto phi = [](double x){
        return 0.5*std::erfc(-x/std::sqrt(2));
    };
    double sqrtT = std::sqrt(option.T);
    double D = std::exp(-market.r * option.T);

    double d1 = (std::log(market.S0/option.K) + (market.r-0.5*market.sigma*market.sigma)*option.T)/(market.sigma*sqrtT);
    double d2 = d1 - market.sigma*sqrtT;

    return market.S0*phi(d1) - option.K*D*phi(d2);
}

/*
European Call
used S_T = S0*exp(drift + vol*Z), where
drift = (r - sigma**2 /2)*T
vol = sigma*sqrt(T)
Z~N(0,1)

Antithetic: simulate Z and -Z and then average their payoffs
*/
inline PricingResult price_european_call(
    const MarketParams &market,
    const EuropeanOptionsParams &option,
    const SimulationConfig &config
) {
    std::mt19937_64 rng(config.seed);
    std::normal_distribution<double> normal(0.0, 1.0);

    const double S0     = market.S0;
    const double r      = market.r;
    const double sigma  = market.sigma;
    const double K      = option.K;
    const double T      = option.T;
    const double drift  = (r - 0.5*sigma*sigma)*T;
    const double vol    = sigma*std::sqrt(T);

    std::vector<double> payoffs;
    
    if(config.antithetic) {
        payoffs.reserve(config.n_sims/2);

        for (int _ = 0; _ < config.n_sims/2; _++) {
            double Z = normal(rng);
            double ST1 = S0*std::exp(drift + vol*Z);
            double ST2 = S0*std::exp(drift - vol*Z);

            double p1 = std::max(ST1 - K, 0.0);
            double p2 = std::max(ST2 - K, 0.0);
            payoffs.push_back(0.5*(p1 + p2));
        }
    }
    else {
        payoffs.reserve(config.n_sims);

        for (int _ = 0; _ < config.n_sims; _++){
            double Z = normal(rng);
            double ST = S0*std::exp(drift + vol*Z);
            payoffs.push_back(std::max(ST - K, 0.0));
        }
    }
    return helper::make_result(payoffs, r, T, config.n_sims, "price_european_call");
}

/*
Asian Call
used payoff = max(mean(path)- K, 0.0), entire path is generated using generate_path function in gbm.hpp
*/
inline PricingResult price_asian_call(
    const MarketParams &market,
    const AsianOptionParams &option,
    const SimulationConfig &config
) {
    std::mt19937_64 rng(config.seed);

    const double K      = option.K;
    const double T      = option.T;

    std::vector<double> payoffs;
    payoffs.reserve(config.n_sims);

    for (int _ = 0; _ < config.n_sims; _++) {
        std::vector<double> path = generate_path(market, T, config.n_steps, rng);
        double n = static_cast<double>(path.size());
        double mean = std::accumulate(path.begin(), path.end(), 0.0) / n;
        payoffs.push_back(std::max(mean - K, 0.0));
    }

    return helper::make_result(payoffs, market.r, T, config.n_sims, "price_asian_call");
}

/*
Barrier Call
used payoff = max(S_T - K, 0.0) if max(path) <= B, else 0
*/
inline PricingResult price_barrier_call(
    const MarketParams &market,
    const BarrierOptionParams &option,
    const SimulationConfig &config
) {
    std::mt19937_64 rng(config.seed);

    const double K = option.K;
    const double T = option.T;
    const double B = option.B;

    std::vector<double> payoffs;
    payoffs.reserve(config.n_sims);

    for (int _ = 0; _ < config.n_sims; _++){
        std::vector<double> path = generate_path(market, T, config.n_steps, rng);
        double M = path[0];
        for (double &x: path) M = std::max(M, x);
        payoffs.push_back(M>B ? 0.0 : std::max(path.back() - K, 0.0));
    }

    return helper::make_result(payoffs, market.r, T, config.n_sims, "price_barrier_call");
}


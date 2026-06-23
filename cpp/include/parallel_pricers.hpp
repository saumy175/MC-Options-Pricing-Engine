#pragma once

#include "gbm.hpp"
#include "pricers.hpp"

#include <future>

/*
Divide n_sims to n_workers batches, (each worker has different RNG seed and worker_id)
A Batch returns payoff_sum and count and main thread combines the result 
*/

namespace helper {
struct BatchResult{
    double payoff_sum;
    double payoff_sum_sq;
    int count;
};
} //namespace helper

/*
Parallel European Call
*/
inline PricingResult price_european_call_parallel(
    const MarketParams &market,
    const EuropeanOptionParams &option,
    const SimulationConfig &config
) {
    const int n_workers = config.n_workers;
    const int n_sims_each = config.n_sims / n_workers;
    const int remainder_sims = config.n_sims % n_workers;

    const double S0     = market.S0;
    const double r      = market.r;
    const double sigma  = market.sigma;
    const double K      = option.K;
    const double T      = option.T;
    const double drift  = (r - 0.5*sigma*sigma)*T;
    const double vol    = sigma*std::sqrt(T);

    auto worker = [=](int n_sims, int seed) -> helper::BatchResult {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> normal(0.0, 1.0);

        double sum = 0.0;
        double sum_squared = 0.0;
        int cnt = 0;

        if(config.antithetic) {
            for(int _ = 0; _ < n_sims/2; _++) {
                double Z = normal(rng);
                double ST1 = S0*std::exp(drift + vol*Z);
                double ST2 = S0*std::exp(drift - vol*Z);

                double p1 = std::max(ST1 - K, 0.0);
                double p2 = std::max(ST2 - K, 0.0);
                double p = 0.5*(p1 + p2);
                sum += p;
                sum_squared += p*p;
                cnt++;
            }
        }
        else {
            for (int _ = 0; _ < n_sims; _++){
                double Z = normal(rng);
                double ST = S0*std::exp(drift + vol*Z);
                double p = std::max(ST - K, 0.0);
                sum += p;
                sum_squared += p*p;
                cnt++;
            }
        }
        
        return {sum, sum_squared, cnt};
    };

    std::vector<std::future<helper::BatchResult>> futures;
    futures.reserve(n_workers);

    for (int w = 0; w < n_workers; w++) {
        int batch = n_sims_each + (w == n_workers-1 ? remainder_sims: 0);
        int seed = config.seed + w;

        futures.push_back(std::async(std::launch::async, worker, batch, seed));
    }

    double total_sum = 0.0;
    double total_sum_squared = 0.0;
    int total_count = 0;
    for (auto &f: futures){
        auto [s, sq, c] = f.get();
        total_sum += s;
        total_sum_squared += sq;
        total_count += c;
    }

    double D = std::exp(-r*T);
    double mean = total_sum/total_count;
    double var = (total_sum_squared - total_sum*mean) / (total_count -1);
    double se = std::sqrt(var/total_count);

    double price = D*(total_sum/total_count);
    double standard_error = D*se;

    return PricingResult{price, standard_error, {price - 1.96*standard_error, price + 1.96*standard_error}, total_count, "price_european_call_parallel"};
}

/*
Parallel Asian Call
*/
inline PricingResult price_asian_call_parallel(
    const MarketParams &market,
    const AsianOptionParams &option,
    const SimulationConfig &config
) {
    const int n_workers = config.n_workers;
    const int n_sims_each = config.n_sims / n_workers;
    const int remainder_sims = config.n_sims % n_workers; 

    auto worker = [=](int n_sims, int seed) -> helper::BatchResult {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> normal(0.0, 1.0);

        double sum = 0.0;
        double sum_squared = 0.0;
        int cnt = 0;

        for(int _ = 0; _ < n_sims; _++){
            std::vector<double> path = generate_path(market, option.T, config.n_steps, rng);
            double n = static_cast<double>(path.size());
            double mean = std::accumulate(path.begin(), path.end(), 0.0) /n;
            double p = std::max(mean - option.K, 0.0);

            sum += p;
            sum_squared += p*p;
            cnt++;
        }

        return {sum, sum_squared, cnt};
    };

    std::vector<std::future<helper::BatchResult>> futures;
    futures.reserve(n_workers);

    for (int w = 0; w < n_workers; w++) {
        int batch = n_sims_each + (w == n_workers-1 ? remainder_sims: 0);
        int seed = config.seed + w;

        futures.push_back(std::async(std::launch::async, worker, batch, seed));
    }

    double total_sum = 0.0;
    double total_sum_squared = 0.0;
    int total_count = 0;
    for (auto &f: futures){
        auto [s, sq, c] = f.get();
        total_sum += s;
        total_sum_squared += sq;
        total_count += c;
    }

    double D = std::exp(-market.r*option.T);
    double mean = total_sum/total_count;
    double var = (total_sum_squared - total_sum*mean) / (total_count -1);
    double se = std::sqrt(var/total_count);

    double price = D*(total_sum/total_count);
    double standard_error = D*se;

    return PricingResult{price, standard_error, {price - 1.96*standard_error, price + 1.96*standard_error}, total_count, "price_asian_call_parallel"};
}

/*
Parallel Barrier Call
*/
inline PricingResult price_barrier_call_parallel(
    const MarketParams &market,
    const BarrierOptionParams &option,
    const SimulationConfig &config
) {
    const int n_workers = config.n_workers;
    const int n_sims_each = config.n_sims / n_workers;
    const int remainder_sims = config.n_sims % n_workers; 

    auto worker = [=](int n_sims, int seed) -> helper::BatchResult {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> normal(0.0, 1.0);

        double sum = 0.0;
        double sum_squared = 0.0;
        int cnt = 0;

        for(int _ = 0; _ < n_sims; _++){
            std::vector<double> path = generate_path(market, option.T, config.n_steps, rng);
            double M = path[0];
            for (double &x: path) M = std::max(M, x);
            double p = (M > option.B ? 0.0: std::max(path.back() - option.K, 0.0));
            sum += p;
            sum_squared += p*p;
            cnt++;
        }

        return {sum, sum_squared, cnt};
    };

    std::vector<std::future<helper::BatchResult>> futures;
    futures.reserve(n_workers);

    for (int w = 0; w < n_workers; w++) {
        int batch = n_sims_each + (w == n_workers-1 ? remainder_sims: 0);
        int seed = config.seed + w;

        futures.push_back(std::async(std::launch::async, worker, batch, seed));
    }

    double total_sum = 0.0;
    double total_sum_squared = 0.0;
    int total_count = 0;
    for (auto &f: futures){
        auto [s, sq, c] = f.get();
        total_sum += s;
        total_sum_squared += sq;
        total_count += c;
    }

    double D = std::exp(-market.r*option.T);
    double mean = total_sum/total_count;
    double var = (total_sum_squared - total_sum*mean) / (total_count -1);
    double se = std::sqrt(var/total_count);

    double price = D*(total_sum/total_count);
    double standard_error = D*se;

    return PricingResult{price, standard_error, {price - 1.96*standard_error, price + 1.96*standard_error}, total_count, "price_barrier_call_parallel"};
}
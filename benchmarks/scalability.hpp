#pragma once

#include "parallel_pricers.hpp"
#include "cli.hpp"
#include "pricing.hpp"

#include <iostream>
#include <iomanip>
#include <vector>

/*
Scalability benchmark

Runs Asian and Barrier pricers at worker counts
{1, 2, 4, 8, 16, 32} and prints a speedup table.

European is excluded: its per-path work is too small (single exp() call) to amortise thread launch overhead.
*/
namespace bench {

inline void run_scalability(const CLIArgs& args) {
    auto market = to_market(args);
    auto as_opt = to_as(args);
    auto ba_opt = to_ba(args);

    std::vector<int> worker_counts = {1, 2, 4, 8, 16, 32};

    std::cout  << "Scalability Benchmark (N=" << args.n_sims  << ", steps=" << args.n_steps << ")\n\n";

    {
        SimulationConfig cfg = to_config(args);
        cfg.n_workers = 1;

        auto t0    = Clock::now();
        price_asian_call_parallel(market, as_opt, cfg);
        double as_base = elapsed_ms(t0);

        t0 = Clock::now();
        price_barrier_call_parallel(market, ba_opt, cfg);
        double ba_base = elapsed_ms(t0);

        std::cout << "\n  Workers | Asian (ms) | Speedup | Barrier (ms) | Speedup\n"
                  << "  --------|------------|---------|--------------|--------\n";

        std::cout << std::fixed
                  << "  " << std::setw(7) << 1
                  << " | " << std::setw(10) << std::setprecision(1) << as_base
                  << " |   1.00x"
                  << " | " << std::setw(12) << ba_base
                  << " |   1.00x\n";

        for (int i = 1; i < static_cast<int>(worker_counts.size()); ++i) {
            int w = worker_counts[i];
            cfg.n_workers = w;

            t0 = Clock::now();
            price_asian_call_parallel(market, as_opt, cfg);
            double as_ms = elapsed_ms(t0);

            t0 = Clock::now();
            price_barrier_call_parallel(market, ba_opt, cfg);
            double ba_ms = elapsed_ms(t0);

            std::cout << "  " << std::setw(7)  << w
                      << " | " << std::setw(10) << std::setprecision(1) << as_ms
                      << " | " << std::setw(6)  << std::setprecision(2)
                      << as_base / as_ms << "x"
                      << " | " << std::setw(12) << std::setprecision(1) << ba_ms
                      << " | " << std::setw(6)  << std::setprecision(2)
                      << ba_base / ba_ms << "x\n";
        }
    }
}

} // namespace bench
#pragma once

#include "pricers.hpp"
#include "cli.hpp"
#include "pricing.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

/*
Convergence benchmark

Prices a European call at increasing N and shows how the estimate and SE evolve.
-Price converges toward the BS reference
-SE shrinks proportional to 1/sqrt(N)
-|MC - BS| shrinks at the same rate

Uses the sequential pricer (single-threaded) so timing reflects pure MC cost, not thread overhead. 
The same seed is used throughout so results are reproducible and monotone in SE.
*/

namespace bench {

inline void run_convergence(const CLIArgs& args) {
    auto market = to_market(args);
    auto eu_opt = to_eu(args);
    auto config = to_config(args);

    double bs = black_scholes_call(market, eu_opt);

    std::vector<int> sim_counts = {
        1'000, 5'000, 10'000, 50'000,
        100'000, 500'000, 1'000'000
    };

    std::cout << "Convergence Benchmark (European call, BS = " << std::fixed << std::setprecision(4) << bs << ")\n\n";

    std::cout << "  N          | Price   | SE     | |MC-BS| | SE ratio\n"
              << "  -----------|---------|--------|---------|----------\n";

    double prev_se = -1.0;

    for (int n : sim_counts) {
        config.n_sims = n;
        auto r = price_european_call(market, eu_opt, config);

        std::string ratio_str = "   —  ";
        if (prev_se > 0) {
            double ratio = prev_se / r.standard_error;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2fx", ratio);
            ratio_str = buf;
        }
        prev_se = r.standard_error;

        std::cout << std::fixed
                  << "  " << std::setw(10) << n
                  << " | " << std::setprecision(4) << r.price
                  << " | " << r.standard_error
                  << " | " << std::abs(r.price - bs)
                  << "  | " << ratio_str << "\n";
    }
}

} // namespace bench
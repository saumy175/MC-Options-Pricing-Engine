#pragma once

#include "pricers.hpp"
#include "parallel_pricers.hpp"
#include "cli.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

namespace bench {

using Clock = std::chrono::high_resolution_clock;

inline double elapsed_ms(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

inline void print_result(const std::string& label,const PricingResult& r, double ms) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n" << label << "\n"
              << "  price : " << r.price << "\n"
              << "  95% CI: [" << r.ci.first << ", " << r.ci.second << "]\n"
              << "  SE    : " << r.standard_error << "\n"
              << std::setprecision(1)
              << "  time  : " << ms << " ms\n";
}

/*
Run pricing benchmark
Prints sequential and parallel prices for all option types and Black-Scholes reference.
*/

template<typename F>
void benchmark(const std::string& label, F&& f) {
    auto t0 = Clock::now();
    auto result = f();
    auto ms = elapsed_ms(t0);
    print_result(label, result, ms);
}

inline void run(const CLIArgs& args) {
    auto market = to_market(args);
    auto eu_opt = to_eu(args);
    auto as_opt = to_as(args);
    auto ba_opt = to_ba(args);
    auto config = to_config(args);

    std::cout << "Pricing Benchmark (N=" << args.n_sims << ", workers=" << args.n_workers << ")\n\n";

    // Black-Scholes reference
    double bs = black_scholes_call(market, eu_opt);
    std::cout << std::fixed << std::setprecision(4) << "Black-Scholes: " << bs << "\n";

    std::cout << "\nSequential: \n";
    benchmark("European", [&]{return price_european_call(market, eu_opt, config);});
    benchmark("Asian", [&]{return price_asian_call(market, as_opt, config);});
    benchmark("Barrier", [&]{return price_barrier_call(market, ba_opt, config);});

    std::cout << "\nParallel (" << args.n_workers << " workers): \n";
    benchmark("European", [&]{return price_european_call_parallel(market, eu_opt, config);});
    benchmark("Asian", [&]{return price_asian_call_parallel(market, as_opt, config);});
    benchmark("Barrier", [&]{return price_barrier_call_parallel(market, ba_opt, config);});
}

} // namespace bench
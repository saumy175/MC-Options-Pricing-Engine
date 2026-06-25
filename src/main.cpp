#include "gbm.hpp"
#include "pricers.hpp"
#include "parallel_pricers.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────

using Clock = std::chrono::high_resolution_clock;

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void print_result(const std::string& label, const PricingResult& r, double ms) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n" << label << "\n";
    std::cout << "  price : " << r.price << "\n";
    std::cout << "  95% CI: [" << r.ci.first << ", " << r.ci.second << "]\n";
    std::cout << "  SE    : " << r.standard_error << "\n";
    std::cout << "  time  : " << std::setprecision(1) << ms << " ms\n";
}

void print_header(const std::string& title) {
    std::cout << "\n══════════════════════════════════\n";
    std::cout << "  " << title << "\n";
    std::cout << "══════════════════════════════════\n";
}

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────

int main() {
    // Standard test parameters — same as Python validation run
    MarketParams market {
        .S0    = 100.0,
        .r     = 0.05,
        .sigma = 0.20
    };

    EuropeanOptionParams eu_opt { .K = 100.0, .T = 1.0 };
    AsianOptionParams    as_opt { .K = 100.0, .T = 1.0 };
    BarrierOptionParams  ba_opt { .K = 100.0, .T = 1.0, .B = 120.0 };

    SimulationConfig config {
        .n_sims     = 1'000'000,
        .n_steps    = 252,
        .n_workers  = 4,
        .seed       = 42,
        .antithetic = true
    };

    // ── Black-Scholes reference ──────────────────
    print_header("Black-Scholes reference");
    double bs = black_scholes_call(market, eu_opt);
    std::cout << "\n  BS price: " << std::fixed << std::setprecision(4) << bs << "\n";

    // ── Single-threaded pricers ──────────────────
    print_header("Single-threaded");

    auto t0 = Clock::now();
    auto eu_seq = price_european_call(market, eu_opt, config);
    print_result("European call", eu_seq, elapsed_ms(t0));

    t0 = Clock::now();
    auto as_seq = price_asian_call(market, as_opt, config);
    print_result("Asian call", as_seq, elapsed_ms(t0));

    t0 = Clock::now();
    auto ba_seq = price_barrier_call(market, ba_opt, config);
    print_result("Barrier call", ba_seq, elapsed_ms(t0));

    // ── Multi-threaded pricers ───────────────────
    print_header("Multi-threaded (" + std::to_string(config.n_workers) + " workers)");

    t0 = Clock::now();
    auto eu_par = price_european_call_parallel(market, eu_opt, config);
    double eu_par_ms = elapsed_ms(t0);
    print_result("European call (parallel)", eu_par, eu_par_ms);

    t0 = Clock::now();
    auto as_par = price_asian_call_parallel(market, as_opt, config);
    double as_par_ms = elapsed_ms(t0);
    print_result("Asian call (parallel)", as_par, as_par_ms);

    t0 = Clock::now();
    auto ba_par = price_barrier_call_parallel(market, ba_opt, config);
    double ba_par_ms = elapsed_ms(t0);
    print_result("Barrier call (parallel)", ba_par, ba_par_ms);

    // ── Speedup summary ──────────────────────────
    print_header("Speedup summary");

    // Re-run sequential for fair comparison (no cache effects from above)
    t0 = Clock::now();
    price_european_call(market, eu_opt, config);
    double eu_seq_ms = elapsed_ms(t0);

    t0 = Clock::now();
    price_asian_call(market, as_opt, config);
    double as_seq_ms = elapsed_ms(t0);

    t0 = Clock::now();
    price_barrier_call(market, ba_opt, config);
    double ba_seq_ms = elapsed_ms(t0);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n  Option   | Sequential | Parallel | Speedup\n";
    std::cout << "  ---------|------------|----------|---------\n";
    std::cout << "  European | " << std::setw(8) << eu_seq_ms << " ms | "
                                 << std::setw(6) << eu_par_ms << " ms | "
                                 << eu_seq_ms / eu_par_ms << "x\n";
    std::cout << "  Asian    | " << std::setw(8) << as_seq_ms << " ms | "
                                 << std::setw(6) << as_par_ms << " ms | "
                                 << as_seq_ms / as_par_ms << "x\n";
    std::cout << "  Barrier  | " << std::setw(8) << ba_seq_ms << " ms | "
                                 << std::setw(6) << ba_par_ms << " ms | "
                                 << ba_seq_ms / ba_par_ms << "x\n";

    std::cout << "\n  Validation (European MC vs Black-Scholes):\n";
    std::cout << "    BS price : " << std::setprecision(4) << bs << "\n";
    std::cout << "    MC price : " << eu_seq.price << "\n";
    std::cout << "    Diff     : " << std::abs(eu_seq.price - bs) << "\n";

    return 0;
}
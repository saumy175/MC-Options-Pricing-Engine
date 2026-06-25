#include "cli.hpp"
#include "greeks.hpp"
#include "pricing.hpp"
#include "scalability.hpp"
#include "convergence.hpp"

#include <iostream>
#include <iomanip>

// ─────────────────────────────────────────────
//  Print parameter summary so every run is
//  self-documenting in terminal output.
// ─────────────────────────────────────────────

void print_params(const CLIArgs& a) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nParameters: \n"
              << "S0=" << a.S0 << "\nK=" << a.K
              << "\nr=" << a.r   << "\nsigma=" << a.sigma
              << "\nT=" << a.T   << "\nB=" << a.B
              << "\nsims=" << a.n_sims
              << "\nsteps=" << a.n_steps
              << "\nworkers=" << a.n_workers
              << "\nseed=" << a.seed
              << "\nantithetic=" << (a.antithetic ? "yes" : "no") << "\n"
              << "\n";
}

int main(int argc, char* argv[]) {
    CLIArgs args;
    try {
        args = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "\n  Error: " << e.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }

    print_params(args);

    auto market = to_market(args);
    auto eu_opt = to_eu(args);
    auto config = to_config(args);

    switch (args.mode) {
        case Mode::Pricing:
            bench::run(args);
            break;

        case Mode::Scalability:
            bench::run_scalability(args);
            break;

        case Mode::Convergence:
            bench::run_convergence(args);
            break;

        case Mode::Greeks:
            print_greeks(market, eu_opt, config);
            break;

        case Mode::All:
            bench::run(args);
            bench::run_convergence(args);
            bench::run_scalability(args);
            print_greeks(market, eu_opt, config);
            break;
    }

    std::cout << "\n";
    return 0;
}
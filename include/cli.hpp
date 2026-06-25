#pragma once

#include "gbm.hpp"
#include <string>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <functional>

/*
CLI Modes
*/

enum class Mode {
    Pricing,
    Scalability,
    Convergence,
    Greeks,
    All,
};

inline Mode parse_mode(const std::string& m) {
    if (m == "pricing") return Mode::Pricing;
    else if (m == "scalability") return Mode::Scalability;
    else if (m == "convergence") return Mode::Convergence;
    else if (m == "greeks") return Mode::Greeks;
    else if (m == "all") return Mode::All;
    throw std::invalid_argument("Unknown mode: " + m);
}

inline const char* mode_to_string(Mode m) {
    switch (m) {
        case Mode::Pricing:     return "pricing";
        case Mode::Scalability: return "scalability";
        case Mode::Convergence: return "convergence";
        case Mode::Greeks:      return "greeks";
        case Mode::All:         return "all";
    }
    return "all";
}

/*
Parsed CLI Arguments 
*/

struct CLIArgs {
    double S0    = 100.0;
    double r     = 0.05;
    double sigma = 0.20;

    double K = 100.0;
    double T = 1.0;
    double B = 120.0;

    int  n_sims     = 1'000'000;
    int  n_steps    = 252;
    int  n_workers  = 4;
    int  seed       = 42;
    bool antithetic = true;

    Mode mode = Mode::All;
};

/*
Usage String
*/

inline void print_usage(const char* prog) {
    std::cout << "\nUsage: " << prog << " [options]\n\n"
              << "Market parameters:\n"
              << "  --S0      <float>   Starting price         (default: 100.0)\n"
              << "  --r       <float>   Risk-free rate         (default: 0.05)\n"
              << "  --sigma   <float>   Volatility             (default: 0.20)\n\n"
              << "Option parameters:\n"
              << "  --K       <float>   Strike price           (default: 100.0)\n"
              << "  --T       <float>   Time to expiry (years) (default: 1.0)\n"
              << "  --B       <float>   Barrier level          (default: 120.0)\n\n"
              << "Simulation parameters:\n"
              << "  --sims    <int>     Number of simulations  (default: 1000000)\n"
              << "  --steps   <int>     Path steps per sim     (default: 252)\n"
              << "  --workers <int>     Parallel workers       (default: 4)\n"
              << "  --seed    <int>     RNG seed               (default: 42)\n"
              << "  --no-antithetic     Disable antithetic variates\n\n"
              << "Mode:\n"
              << "  --mode    <string>  pricing | scalability | convergence | greeks | all\n"
              << "                      (default: all)\n\n"
              << "Examples:\n"
              << "  " << prog << "\n"
              << "  " << prog << " --mode pricing --S0 110 --K 105 --sims 500000\n"
              << "  " << prog << " --mode greeks\n"
              << "  " << prog << " --mode scalability --sims 2000000\n\n";
}

/*
Parser (using table)
*/

struct OptionSpec {
    bool takes_value;
    std::function<void(CLIArgs&, const std::string&)> apply;
};

inline CLIArgs parse_args(int argc, char* argv[]) {
    CLIArgs args;

    const std::unordered_map<std::string, OptionSpec> specs = {
        {"--S0", {true,  [](CLIArgs& a, const std::string& v){ a.S0 = std::stod(v); }}},
        {"--r", {true,  [](CLIArgs& a, const std::string& v){ a.r = std::stod(v); }}},
        {"--sigma", {true,  [](CLIArgs& a, const std::string& v){ a.sigma = std::stod(v); }}},
        {"--K", {true,  [](CLIArgs& a, const std::string& v){ a.K = std::stod(v); }}},
        {"--T", {true,  [](CLIArgs& a, const std::string& v){ a.T = std::stod(v); }}},
        {"--B", {true,  [](CLIArgs& a, const std::string& v){ a.B = std::stod(v); }}},
        {"--sims", {true,  [](CLIArgs& a, const std::string& v){ a.n_sims = std::stoi(v); }}},
        {"--steps", {true,  [](CLIArgs& a, const std::string& v){ a.n_steps = std::stoi(v); }}},
        {"--workers", {true,  [](CLIArgs& a, const std::string& v){ a.n_workers = std::stoi(v); }}},
        {"--seed", {true,  [](CLIArgs& a, const std::string& v){ a.seed = std::stoi(v); }}},
        {"--mode", {true,  [](CLIArgs& a, const std::string& v){ a.mode = parse_mode(v); }}},
        {"--no-antithetic", {false, [](CLIArgs& a, const std::string&){ a.antithetic = false; }}},
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        auto it = specs.find(arg);
        if (it == specs.end()) {
            throw std::invalid_argument("Unknown argument: " + arg);
        }

        const OptionSpec& spec = it->second;

        if (!spec.takes_value) {
            spec.apply(args, "");
            continue;
        }

        if (i + 1 >= argc) {
            throw std::invalid_argument(arg + " requires a value");
        }

        std::string value = argv[++i];
        spec.apply(args, value);
    }

    // trivial validation
    if (args.S0 <= 0) throw std::invalid_argument("S0 must be positive");
    if (args.sigma <= 0) throw std::invalid_argument("sigma must be positive");
    if (args.T <= 0) throw std::invalid_argument("T must be positive");
    if (args.K <= 0) throw std::invalid_argument("K must be positive");
    if (args.n_sims <= 0) throw std::invalid_argument("sims must be positive");
    if (args.n_steps <= 0) throw std::invalid_argument("steps must be positive");
    if (args.n_workers <= 0) throw std::invalid_argument("workers must be positive");

    return args;
}

/*
Making Parameter from CLIArgs
*/
inline MarketParams to_market(const CLIArgs& a) { return {a.S0, a.r, a.sigma}; }
inline EuropeanOptionParams to_eu(const CLIArgs& a) { return {a.K, a.T}; }
inline AsianOptionParams to_as(const CLIArgs& a) { return {a.K, a.T}; }
inline BarrierOptionParams to_ba(const CLIArgs& a) { return {a.K, a.T, a.B}; }

inline SimulationConfig to_config(const CLIArgs& a) {
    return {a.n_sims, a.n_steps, a.n_workers, a.seed, a.antithetic};
}
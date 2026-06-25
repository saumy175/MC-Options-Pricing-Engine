#pragma once

#include "pricers.hpp"
#include <iostream>
#include <iomanip>

struct Greeks{
    double delta; // sensitivity to stock prices
    double gamma; // sensitivity of delta to stock price
    double vega; // sensitivity of volatility
    double theta; // sensitivity of time decay
};

Greeks compute_greeks(
    const MarketParams&         market,
    const EuropeanOptionParams& option,
    const SimulationConfig&     config)
{
    const double dS    = 0.01*market.S0;
    const double dsigma  = 0.001;
    const double dT    = 1.0/config.n_steps;

    MarketParams m_up   = {market.S0 + dS, market.r, market.sigma}; // S0 + dS
    MarketParams m_down = {market.S0 - dS, market.r, market.sigma}; // S0 - dS

    double V_up   = price_european_call(m_up,   option, config).price;
    double V_down = price_european_call(m_down, option, config).price;
    double V_base = price_european_call(market, option, config).price;

    double delta = (V_up - V_down) / (2.0 * dS); // dS -> 0 then delta -> d(price)/dS0
    double gamma = (V_up - 2.0 * V_base + V_down) / (dS * dS); // dS -> 0 then gamma -> d^2(price)/d(S0)^2

    MarketParams m_sig_up   = {market.S0, market.r, market.sigma + dsigma}; // sigma + dsigma
    MarketParams m_sig_down = {market.S0, market.r, market.sigma - dsigma}; // sigma - dsigma

    double V_sig_up   = price_european_call(m_sig_up,   option, config).price;
    double V_sig_down = price_european_call(m_sig_down, option, config).price;

    double vega = (V_sig_up - V_sig_down) / (2.0 * dsigma) * 0.01; // dsigma -> 0 then vega -> d(price)/dsigma * 0.01 (reported at per 1%)

    EuropeanOptionParams opt_near = {option.K, option.T - dT}; 
    double V_near = price_european_call(market, opt_near, config).price; // T - dT

    double theta = (V_near - V_base)/dT/config.n_steps; // dT -> 0 then theta -> d(price)/dT

    return {delta, gamma, vega, theta};
}


/*
Greeks for Black-Scholes Model
refer: https://en.wikipedia.org/wiki/Black%E2%80%93Scholes_model#The_Options_Greeks
*/
 
inline Greeks bs_greeks(
    const MarketParams &market,
    const EuropeanOptionParams &option)
{
    auto Phi = [](double x) {
        return 0.5 * std::erfc(-x/std::sqrt(2.0));
    };
    auto phi = [](double x) {
        return std::exp(-0.5*x*x)/std::sqrt(2.0 * M_PI);
    };
 
    double sqrtT = std::sqrt(option.T);
    double d1 = (std::log(market.S0/option.K) + (market.r + 0.5*market.sigma*market.sigma)*option.T)/(market.sigma*sqrtT);
    double d2 = d1 - market.sigma*sqrtT;
 
    double delta = Phi(d1);
    double gamma = phi(d1) / (market.S0*market.sigma*sqrtT);
    double vega  = market.S0*phi(d1)*sqrtT*0.01;  
    double theta = (-(market.S0*phi(d1)*market.sigma)/(2.0*sqrtT) - market.r*option.K*std::exp(-market.r*option.T)*Phi(d2))/365.0;
 
    return {delta, gamma, vega, theta};
}
 
inline void print_greeks(
    const MarketParams &market,
    const EuropeanOptionParams &option,
    const SimulationConfig &config)
{
    std::cout << "\n  Computing Greeks\n";
    Greeks mc = compute_greeks(market, option, config);
    Greeks bs = bs_greeks(market, option);
 
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n  Greek  | MC (finite diff) | Black-Scholes | Error\n";
    std::cout <<   "  -------|------------------|---------------|--------\n";
 
    auto row = [&](const char* name, double mc_val, double bs_val) {
        std::cout << "  " << std::setw(6) << std::left << name
                  << " | " << std::setw(16) << std::right << mc_val
                  << " | " << std::setw(13) << bs_val
                  << " | " << std::abs(mc_val - bs_val) << "\n";
    };
 
    row("Delta", mc.delta, bs.delta);
    row("Gamma", mc.gamma, bs.gamma);
    row("Vega",  mc.vega,  bs.vega);
    row("Theta", mc.theta, bs.theta);
}

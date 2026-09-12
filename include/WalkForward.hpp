#pragma once

#include "Candle.hpp"
#include "Backtester.hpp"

#include <cstddef>
#include <vector>

struct WalkForwardResult
{
    std::size_t trainStart{};
    std::size_t trainEnd{};
    std::size_t testStart{};
    std::size_t testEnd{};

    std::size_t fastPeriod{};
    std::size_t slowPeriod{};
    std::size_t rsiPeriod{};
    double rsiBuyThreshold{};

    double testProfitLoss{};
    double testFinalCapital{};
    double testMaximumDrawdownPercentage{};
    double testWinRate{};
    double testProfitFactor{};
    double testSharpeRatio{};
    std::size_t testTrades{};
};

std::vector<WalkForwardResult> runWalkForwardValidation(
    const std::vector<Candle>& candles,
    std::size_t trainSize,
    std::size_t testSize,
    const std::vector<std::size_t>& fastPeriods,
    const std::vector<std::size_t>& slowPeriods,
    const std::vector<std::size_t>& rsiPeriods,
    const std::vector<double>& rsiThresholds,
    double stopLossPercentage,
    double takeProfitPercentage,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
);
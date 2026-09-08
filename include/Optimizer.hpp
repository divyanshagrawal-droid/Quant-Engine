#pragma once

#include "Candle.hpp"

#include <cstddef>
#include <vector>

struct OptimizationResult
{
    std::size_t fastPeriod{};
    std::size_t slowPeriod{};

    double totalProfitLoss{};
    double finalCapital{};

    double maximumDrawdown{};
    double maximumDrawdownPercentage{};

    double winRate{};
    double profitFactor{};
    double sharpeRatio{};

    std::size_t totalTrades{};
};

std::vector<OptimizationResult> optimizeSMA(
    const std::vector<Candle>& candles,
    const std::vector<std::size_t>& fastPeriods,
    const std::vector<std::size_t>& slowPeriods,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
);
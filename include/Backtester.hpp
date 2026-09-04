#pragma once

#include "Candle.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct Trade {
    std::string entryTime;
    std::string exitTime;

    double entryPrice{};
    double exitPrice{};

    double quantity{};

    double entryFee{};
    double exitFee{};

    double grossProfitLoss{};
    double totalFees{};
    double profitLoss{};
};

struct BacktestResult {
    double initialCapital{};
    double finalCapital{};
    double totalProfitLoss{};

    std::vector<Trade> trades;
};

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
);
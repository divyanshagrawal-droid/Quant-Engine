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

struct EquityPoint {
    std::string timestamp;
    double equity{};
};

struct BacktestResult {
    double initialCapital{};
    double finalCapital{};
    double totalProfitLoss{};

    double maximumDrawdown{};
    double maximumDrawdownPercentage{};

    std::size_t winningTrades{};
    std::size_t losingTrades{};

    double winRate{};
    double averageWin{};
    double averageLoss{};
    double profitFactor{};

    std::vector<Trade> trades;
    std::vector<EquityPoint> equityCurve;
};

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
);
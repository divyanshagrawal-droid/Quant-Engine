#pragma once
#include "Candle.hpp"
#include <cstddef>
#include <vector>

enum class Signal {
    HOLD,
    BUY,
    SELL
};

enum class StrategyType {
    SMA_CROSSOVER,
    RSI_SMA_TREND
};

Signal generateSignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod
);

Signal generateRSISMASignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double rsiBuyThreshold
);

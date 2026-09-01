#pragma once

#include "Candle.hpp"

#include <vector>

enum class Signal {
    HOLD,
    BUY,
    SELL
};

Signal generateSignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod
);
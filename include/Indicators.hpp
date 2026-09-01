#pragma once

#include "Candle.hpp"

#include <vector>

double calculateSMA(
    const std::vector<Candle>& candles,
    std::size_t endIndex,
    std::size_t period
);
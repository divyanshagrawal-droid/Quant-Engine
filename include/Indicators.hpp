#pragma once
#include "Candle.hpp"
#include <cstddef>
#include <vector>

double calculateSMA(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
);

double calculateRSI(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
);

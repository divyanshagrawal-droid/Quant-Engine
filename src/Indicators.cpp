#include "../include/Indicators.hpp"

#include <stdexcept>

double calculateSMA(
    const std::vector<Candle>& candles,
    std::size_t endIndex,
    std::size_t period
) {
    if (period == 0) {
        throw std::invalid_argument("SMA period cannot be zero.");
    }

    if (endIndex >= candles.size()) {
        throw std::out_of_range("End index is outside candle data.");
    }

    if (endIndex + 1 < period) {
        throw std::invalid_argument("Not enough candles for this SMA period.");
    }

    double sum = 0.0;

    const std::size_t startIndex = endIndex + 1 - period;

    for (std::size_t i = startIndex; i <= endIndex; ++i) {
        sum += candles[i].close;
    }

    return sum / static_cast<double>(period);
}
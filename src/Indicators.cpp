#include "../include/Indicators.hpp"

#include <cmath>
#include <stdexcept>

double calculateSMA(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
) {
    if (period == 0 || index >= candles.size() || index + 1 < period) {
        return 0.0;
    }

    double sum = 0.0;

    for (std::size_t i = index + 1 - period; i <= index; ++i) {
        sum += candles[i].close;
    }

    return sum / static_cast<double>(period);
}

// Wilder RSI.
// Returns 0 until enough candles exist to calculate the first RSI value.
double calculateRSI(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
) {
    if (period == 0 || index >= candles.size() || index < period) {
        return 0.0;
    }

    double gainSum = 0.0;
    double lossSum = 0.0;

    // First Wilder average uses the first 'period' price changes.
    for (std::size_t i = 1; i <= period; ++i) {
        const double change =
            candles[i].close - candles[i - 1].close;

        if (change > 0.0) {
            gainSum += change;
        } else {
            lossSum -= change;
        }
    }

    double averageGain =
        gainSum / static_cast<double>(period);

    double averageLoss =
        lossSum / static_cast<double>(period);

    if (index == period) {
        if (averageLoss == 0.0) {
            return averageGain > 0.0 ? 100.0 : 50.0;
        }

        const double rs = averageGain / averageLoss;
        return 100.0 - (100.0 / (1.0 + rs));
    }

    // Continue Wilder smoothing up to the requested index.
    for (std::size_t i = period + 1; i <= index; ++i) {
        const double change =
            candles[i].close - candles[i - 1].close;

        const double gain = change > 0.0 ? change : 0.0;
        const double loss = change < 0.0 ? -change : 0.0;

        averageGain =
            ((averageGain * static_cast<double>(period - 1)) + gain)
            / static_cast<double>(period);

        averageLoss =
            ((averageLoss * static_cast<double>(period - 1)) + loss)
            / static_cast<double>(period);
    }

    if (averageLoss == 0.0) {
        return averageGain > 0.0 ? 100.0 : 50.0;
    }

    const double rs = averageGain / averageLoss;

    return 100.0 - (100.0 / (1.0 + rs));
}

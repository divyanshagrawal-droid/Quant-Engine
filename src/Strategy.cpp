#include "../include/Strategy.hpp"
#include "../include/Indicators.hpp"

Signal generateSignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod
) {
    // Need two points in time to detect a crossover.
    if (index < slowPeriod) {
        return Signal::HOLD;
    }

    if (index == 0) {
        return Signal::HOLD;
    }

    const double currentFast =
        calculateSMA(candles, index, fastPeriod);

    const double currentSlow =
        calculateSMA(candles, index, slowPeriod);

    const double previousFast =
        calculateSMA(candles, index - 1, fastPeriod);

    const double previousSlow =
        calculateSMA(candles, index - 1, slowPeriod);

    // Bullish crossover
    if (previousFast <= previousSlow &&
        currentFast > currentSlow) {
        return Signal::BUY;
    }

    // Bearish crossover
    if (previousFast >= previousSlow &&
        currentFast < currentSlow) {
        return Signal::SELL;
    }

    return Signal::HOLD;
}
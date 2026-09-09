#include "../include/Strategy.hpp"
#include "../include/Indicators.hpp"

Signal generateSignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod
) {
    if (fastPeriod == 0 || slowPeriod == 0 ||
        fastPeriod >= slowPeriod ||
        index < slowPeriod ||
        index == 0) {
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

    if (previousFast <= previousSlow &&
        currentFast > currentSlow) {
        return Signal::BUY;
    }

    if (previousFast >= previousSlow &&
        currentFast < currentSlow) {
        return Signal::SELL;
    }

    return Signal::HOLD;
}

Signal generateRSISMASignal(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double rsiBuyThreshold
) {
    if (fastPeriod == 0 || slowPeriod == 0 ||
        rsiPeriod == 0 ||
        fastPeriod >= slowPeriod ||
        index == 0 ||
        index < slowPeriod ||
        index < rsiPeriod) {
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

    const double rsi =
        calculateRSI(candles, index, rsiPeriod);

    const bool bullishCross =
        previousFast <= previousSlow &&
        currentFast > currentSlow;

    const bool bearishCross =
        previousFast >= previousSlow &&
        currentFast < currentSlow;

    // Entry requires:
    // 1. Bullish SMA crossover
    // 2. Price above the slow SMA (trend confirmation)
    // 3. RSI above the configurable momentum threshold
    if (bullishCross &&
        candles[index].close > currentSlow &&
        rsi >= rsiBuyThreshold) {
        return Signal::BUY;
    }

    // Exit when the trend breaks.
    if (bearishCross ||
        candles[index].close < currentSlow) {
        return Signal::SELL;
    }

    return Signal::HOLD;
}

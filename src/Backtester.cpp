#include "../include/Backtester.hpp"
#include "../include/Strategy.hpp"

#include <vector>

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital
) {
    BacktestResult result;

    result.initialCapital = initialCapital;
    result.finalCapital = initialCapital;

    bool inPosition = false;

    double quantity = 0.0;
    double entryPrice = 0.0;

    std::string entryTime;

    for (std::size_t i = 0; i < candles.size(); ++i) {

        const Signal signal =
            generateSignal(
                candles,
                i,
                fastPeriod,
                slowPeriod
            );

        // BUY: Open a position
        if (signal == Signal::BUY && !inPosition) {

            entryPrice = candles[i].close;
            entryTime = candles[i].timestamp;

            // Use all available capital for this first simple version.
            quantity = result.finalCapital / entryPrice;

            inPosition = true;
        }

        // SELL: Close the position
        else if (signal == Signal::SELL && inPosition) {

            const double exitPrice = candles[i].close;

            const double profitLoss =
                (exitPrice - entryPrice) * quantity;

            Trade trade;

            trade.entryTime = entryTime;
            trade.exitTime = candles[i].timestamp;

            trade.entryPrice = entryPrice;
            trade.exitPrice = exitPrice;

            trade.quantity = quantity;
            trade.profitLoss = profitLoss;

            result.trades.push_back(trade);

            result.finalCapital += profitLoss;

            inPosition = false;
            quantity = 0.0;
            entryPrice = 0.0;
            entryTime.clear();
        }
    }
    // Close any position that is still open at the end
// of the historical data.
if (inPosition && !candles.empty()) {

    const Candle& finalCandle = candles.back();

    const double exitPrice = finalCandle.close;

    const double profitLoss =
        (exitPrice - entryPrice) * quantity;

    Trade trade;

    trade.entryTime = entryTime;
    trade.exitTime = finalCandle.timestamp;

    trade.entryPrice = entryPrice;
    trade.exitPrice = exitPrice;

    trade.quantity = quantity;
    trade.profitLoss = profitLoss;

    result.trades.push_back(trade);

    result.finalCapital += profitLoss;

    // Position is now closed.
    inPosition = false;
    quantity = 0.0;
    entryPrice = 0.0;
    entryTime.clear();
}

    result.totalProfitLoss =
        result.finalCapital - result.initialCapital;

    return result;
}
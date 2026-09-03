#include "../include/Backtester.hpp"
#include "../include/Strategy.hpp"

#include <vector>

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
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

        // ==================================================
        // BUY: Open a position
        // ==================================================
        if (signal == Signal::BUY && !inPosition) {

            const double marketPrice = candles[i].close;

            // Buying suffers positive slippage.
            entryPrice =
                marketPrice * (1.0 + slippageRate);

            entryTime = candles[i].timestamp;

            // Fee is charged on the purchase.
            const double entryFee =
                result.finalCapital * tradingFeeRate;

            const double capitalAfterFee =
                result.finalCapital - entryFee;

            quantity =
                capitalAfterFee / entryPrice;

            inPosition = true;
        }

        // ==================================================
        // SELL: Close the position
        // ==================================================
        else if (signal == Signal::SELL && inPosition) {

            const double marketPrice = candles[i].close;

            // Selling suffers negative slippage.
            const double exitPrice =
                marketPrice * (1.0 - slippageRate);

            const double grossProceeds =
                exitPrice * quantity;

            // Fee is charged on the sale.
            const double exitFee =
                grossProceeds * tradingFeeRate;

            const double netProceeds =
                grossProceeds - exitFee;

            const double investedCapital =
                entryPrice * quantity;

            const double profitLoss =
                netProceeds - investedCapital;

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

    // ======================================================
    // FIX:
    // Close any position still open at the end of the data.
    // ======================================================
    if (inPosition && !candles.empty()) {

        const Candle& finalCandle = candles.back();

        const double marketPrice = finalCandle.close;

        // Selling suffers negative slippage.
        const double exitPrice =
            marketPrice * (1.0 - slippageRate);

        const double grossProceeds =
            exitPrice * quantity;

        const double exitFee =
            grossProceeds * tradingFeeRate;

        const double netProceeds =
            grossProceeds - exitFee;

        const double investedCapital =
            entryPrice * quantity;

        const double profitLoss =
            netProceeds - investedCapital;

        Trade trade;

        trade.entryTime = entryTime;
        trade.exitTime = finalCandle.timestamp;

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

    result.totalProfitLoss =
        result.finalCapital - result.initialCapital;

    return result;
}
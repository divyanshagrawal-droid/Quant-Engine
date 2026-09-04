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
    double entryFee = 0.0;

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

            // Positive slippage when buying.
            entryPrice =
                marketPrice * (1.0 + slippageRate);

            entryTime = candles[i].timestamp;

            // Use available capital while reserving
            // enough money for the entry fee.
            quantity =
                result.finalCapital /
                (entryPrice * (1.0 + tradingFeeRate));

            // Actual entry fee.
            const double entryValue =
                entryPrice * quantity;

            entryFee =
                entryValue * tradingFeeRate;

            inPosition = true;
        }

        // ==================================================
        // SELL: Close the position
        // ==================================================
        else if (signal == Signal::SELL && inPosition) {

            const double marketPrice = candles[i].close;

            // Negative slippage when selling.
            const double exitPrice =
                marketPrice * (1.0 - slippageRate);

            const double exitValue =
                exitPrice * quantity;

            // Exit fee.
            const double exitFee =
                exitValue * tradingFeeRate;

            // Price movement only, before fees.
            const double grossProfitLoss =
                (exitPrice - entryPrice) * quantity;

            // Total trading costs.
            const double totalFees =
                entryFee + exitFee;

            // Final trade result.
            const double profitLoss =
                grossProfitLoss - totalFees;

            Trade trade;

            trade.entryTime = entryTime;
            trade.exitTime = candles[i].timestamp;

            trade.entryPrice = entryPrice;
            trade.exitPrice = exitPrice;

            trade.quantity = quantity;

            trade.entryFee = entryFee;
            trade.exitFee = exitFee;

            trade.grossProfitLoss = grossProfitLoss;
            trade.totalFees = totalFees;
            trade.profitLoss = profitLoss;

            result.trades.push_back(trade);

            // Update portfolio capital.
            result.finalCapital += profitLoss;

            inPosition = false;

            quantity = 0.0;
            entryPrice = 0.0;
            entryFee = 0.0;

            entryTime.clear();
        }
    }

    // ======================================================
    // Close any position still open at the end of the data.
    // ======================================================
    if (inPosition && !candles.empty()) {

        const Candle& finalCandle = candles.back();

        const double marketPrice =
            finalCandle.close;

        // Negative slippage when selling.
        const double exitPrice =
            marketPrice * (1.0 - slippageRate);

        const double exitValue =
            exitPrice * quantity;

        // Exit fee.
        const double exitFee =
            exitValue * tradingFeeRate;

        // Price movement only, before fees.
        const double grossProfitLoss =
            (exitPrice - entryPrice) * quantity;

        // Total trading costs.
        const double totalFees =
            entryFee + exitFee;

        // Final trade result.
        const double profitLoss =
            grossProfitLoss - totalFees;

        Trade trade;

        trade.entryTime = entryTime;
        trade.exitTime = finalCandle.timestamp;

        trade.entryPrice = entryPrice;
        trade.exitPrice = exitPrice;

        trade.quantity = quantity;

        trade.entryFee = entryFee;
        trade.exitFee = exitFee;

        trade.grossProfitLoss = grossProfitLoss;
        trade.totalFees = totalFees;
        trade.profitLoss = profitLoss;

        result.trades.push_back(trade);

        result.finalCapital += profitLoss;

        inPosition = false;

        quantity = 0.0;
        entryPrice = 0.0;
        entryFee = 0.0;

        entryTime.clear();
    }

    result.totalProfitLoss =
        result.finalCapital - result.initialCapital;

    return result;
}
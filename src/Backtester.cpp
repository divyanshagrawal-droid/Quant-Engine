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

    // =========================================================
    // Process every candle
    // =========================================================
    for (std::size_t i = 0; i < candles.size(); ++i) {

        const Signal signal =
            generateSignal(
                candles,
                i,
                fastPeriod,
                slowPeriod
            );

        // =====================================================
        // BUY: Open a new position
        // =====================================================
        if (signal == Signal::BUY && !inPosition) {

            const double marketPrice = candles[i].close;

            // Positive slippage when buying.
            entryPrice =
                marketPrice * (1.0 + slippageRate);

            entryTime = candles[i].timestamp;

            // We use the available capital while reserving
            // enough money to pay the entry fee.
            quantity =
                result.finalCapital /
                (entryPrice * (1.0 + tradingFeeRate));

            // Actual entry value.
            const double entryValue =
                entryPrice * quantity;

            // Actual entry fee.
            entryFee =
                entryValue * tradingFeeRate;

            inPosition = true;
        }

        // =====================================================
        // SELL: Close the existing position
        // =====================================================
        else if (signal == Signal::SELL && inPosition) {

            const double marketPrice = candles[i].close;

            // Negative slippage when selling.
            const double exitPrice =
                marketPrice * (1.0 - slippageRate);

            // Value received from selling the position.
            const double exitValue =
                exitPrice * quantity;

            // Fee charged on the sale.
            const double exitFee =
                exitValue * tradingFeeRate;

            // P&L caused only by price movement.
            const double grossProfitLoss =
                (exitPrice - entryPrice) * quantity;

            // Total fees paid for this trade.
            const double totalFees =
                entryFee + exitFee;

            // Final net P&L.
            const double profitLoss =
                grossProfitLoss - totalFees;

            // Store trade details.
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

            // Update portfolio capital using net P&L.
            result.finalCapital += profitLoss;

            // Reset position state.
            inPosition = false;

            quantity = 0.0;
            entryPrice = 0.0;
            entryFee = 0.0;

            entryTime.clear();
        }

        // =====================================================
        // EQUITY CURVE
        // =====================================================
        //
        // Equity = remaining cash + current market value
        // of any open position.
        //
        // This avoids double-counting the position value.
        //
        double currentEquity = result.finalCapital;

        if (inPosition) {

            // Value of the position at the current market price.
            const double currentPositionValue =
                candles[i].close * quantity;

            // Amount of capital used to acquire the position.
            const double entryValue =
                entryPrice * quantity;

            // Cash remaining after buying the position
            // and paying the entry fee.
            const double cashRemaining =
                result.finalCapital
                - entryValue
                - entryFee;

            currentEquity =
                cashRemaining
                + currentPositionValue;
        }

        EquityPoint equityPoint;

        equityPoint.timestamp =
            candles[i].timestamp;

        equityPoint.equity =
            currentEquity;

        result.equityCurve.push_back(
            equityPoint
        );
    }

    // =========================================================
    // CLOSE OPEN POSITION AT END OF HISTORICAL DATA
    // =========================================================
    //
    // If the final signal was BUY and no later SELL occurred,
    // close the position at the final candle's market price.
    //
    if (inPosition && !candles.empty()) {

        const Candle& finalCandle =
            candles.back();

        const double marketPrice =
            finalCandle.close;

        // Negative slippage when selling.
        const double exitPrice =
            marketPrice * (1.0 - slippageRate);

        // Value received from selling.
        const double exitValue =
            exitPrice * quantity;

        // Exit fee.
        const double exitFee =
            exitValue * tradingFeeRate;

        // Gross price-movement P&L.
        const double grossProfitLoss =
            (exitPrice - entryPrice) * quantity;

        // Total fees.
        const double totalFees =
            entryFee + exitFee;

        // Net P&L.
        const double profitLoss =
            grossProfitLoss - totalFees;

        Trade trade;

        trade.entryTime =
            entryTime;

        trade.exitTime =
            finalCandle.timestamp;

        trade.entryPrice =
            entryPrice;

        trade.exitPrice =
            exitPrice;

        trade.quantity =
            quantity;

        trade.entryFee =
            entryFee;

        trade.exitFee =
            exitFee;

        trade.grossProfitLoss =
            grossProfitLoss;

        trade.totalFees =
            totalFees;

        trade.profitLoss =
            profitLoss;

        result.trades.push_back(
            trade
        );

        // Update final portfolio capital.
        result.finalCapital +=
            profitLoss;

        // Reset position.
        inPosition = false;

        quantity = 0.0;
        entryPrice = 0.0;
        entryFee = 0.0;

        entryTime.clear();

        // The final candle's equity should match
        // the final realized portfolio capital.
        if (!result.equityCurve.empty()) {

            result.equityCurve.back().equity =
                result.finalCapital;
        }
    }

    // =========================================================
    // FINAL RESULT
    // =========================================================

    result.totalProfitLoss =
        result.finalCapital
        - result.initialCapital;

    return result;
}
#include "../include/Backtester.hpp"
#include "../include/Strategy.hpp"

#include <vector>
#include <cmath>

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
    // PROCESS HISTORICAL CANDLES
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
        // BUY: OPEN POSITION
        // =====================================================

        if (signal == Signal::BUY && !inPosition) {

            const double marketPrice =
                candles[i].close;

            // Buying suffers positive slippage.
            entryPrice =
                marketPrice * (1.0 + slippageRate);

            entryTime =
                candles[i].timestamp;

            // Reserve enough capital for the
            // entry trading fee.
            quantity =
                result.finalCapital /
                (entryPrice * (1.0 + tradingFeeRate));

            // Calculate actual entry value.
            const double entryValue =
                entryPrice * quantity;

            // Calculate entry fee.
            entryFee =
                entryValue * tradingFeeRate;

            inPosition = true;
        }

        // =====================================================
        // SELL: CLOSE POSITION
        // =====================================================

        else if (signal == Signal::SELL && inPosition) {

            const double marketPrice =
                candles[i].close;

            // Selling suffers negative slippage.
            const double exitPrice =
                marketPrice * (1.0 - slippageRate);

            // Value received from selling.
            const double exitValue =
                exitPrice * quantity;

            // Exit fee.
            const double exitFee =
                exitValue * tradingFeeRate;

            // Gross P&L from price movement.
            const double grossProfitLoss =
                (exitPrice - entryPrice) * quantity;

            // Total trading costs.
            const double totalFees =
                entryFee + exitFee;

            // Net P&L after all fees.
            const double profitLoss =
                grossProfitLoss - totalFees;

            // Store trade details.
            Trade trade;

            trade.entryTime =
                entryTime;

            trade.exitTime =
                candles[i].timestamp;

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

            result.trades.push_back(trade);

            // Update portfolio capital.
            result.finalCapital +=
                profitLoss;

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

        double currentEquity =
            result.finalCapital;

        if (inPosition) {

            // Current market value of the position.
            const double currentPositionValue =
                candles[i].close * quantity;

            // Amount spent when entering.
            const double entryValue =
                entryPrice * quantity;

            // Cash remaining after entry.
            const double cashRemaining =
                result.finalCapital
                - entryValue
                - entryFee;

            // Portfolio equity =
            // remaining cash + current position value.
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
    // CLOSE POSITION AT END OF DATA
    // =========================================================

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

        // Gross P&L from price movement.
        const double grossProfitLoss =
            (exitPrice - entryPrice) * quantity;

        // Total fees.
        const double totalFees =
            entryFee + exitFee;

        // Net P&L.
        const double profitLoss =
            grossProfitLoss - totalFees;

        // Store final trade.
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

        // Final equity must match final capital
        // after the position has been realized.
        if (!result.equityCurve.empty()) {

            result.equityCurve.back().equity =
                result.finalCapital;
        }
    }

    // =========================================================
    // TOTAL P&L
    // =========================================================

    result.totalProfitLoss =
        result.finalCapital
        - result.initialCapital;

    // =========================================================
    // MAXIMUM DRAWDOWN
    // =========================================================

    double peakEquity =
        result.initialCapital;

    double maximumDrawdown = 0.0;

    double maximumDrawdownPercentage = 0.0;

    for (const auto& point : result.equityCurve) {

        // Update peak equity.
        if (point.equity > peakEquity) {
            peakEquity =
                point.equity;
        }

        // Calculate current drawdown.
        const double drawdown =
            peakEquity - point.equity;

        // Update maximum drawdown in money.
        if (drawdown > maximumDrawdown) {
            maximumDrawdown =
                drawdown;
        }

        // Calculate drawdown percentage.
        if (peakEquity > 0.0) {

            const double drawdownPercentage =
                (drawdown / peakEquity) * 100.0;

            if (drawdownPercentage >
                maximumDrawdownPercentage) {

                maximumDrawdownPercentage =
                    drawdownPercentage;
            }
        }
    }

    result.maximumDrawdown =
        maximumDrawdown;

    result.maximumDrawdownPercentage =
        maximumDrawdownPercentage;

        // =========================================================
// TRADE STATISTICS
// =========================================================

double totalWinningProfit = 0.0;
double totalLosingProfit = 0.0;

for (const auto& trade : result.trades) {

    if (trade.profitLoss > 0.0) {

        ++result.winningTrades;

        totalWinningProfit += trade.profitLoss;
    }
    else if (trade.profitLoss < 0.0) {

        ++result.losingTrades;

        totalLosingProfit += trade.profitLoss;
    }
}

// Win rate.
if (!result.trades.empty()) {

    result.winRate =
        (static_cast<double>(result.winningTrades) /
         static_cast<double>(result.trades.size())) * 100.0;
}

// Average winning trade.
if (result.winningTrades > 0) {

    result.averageWin =
        totalWinningProfit /
        static_cast<double>(result.winningTrades);
}

// Average losing trade.
if (result.losingTrades > 0) {

    result.averageLoss =
        totalLosingProfit /
        static_cast<double>(result.losingTrades);
}

// =========================================================
// PROFIT FACTOR
// =========================================================

// Profit Factor = Gross Profit / Absolute Gross Loss

if (totalLosingProfit < 0.0) {

    result.profitFactor =
        totalWinningProfit /
        (-totalLosingProfit);
}
else if (totalWinningProfit > 0.0) {

    // No losing trades.
    result.profitFactor =
        999999.0;
}
else {

    // No winning or losing trades.
    result.profitFactor =
        0.0;
}

// =========================================================
// SHARPE RATIO
// =========================================================

if (result.equityCurve.size() >= 2) {

    std::vector<double> returns;

    returns.reserve(
        result.equityCurve.size() - 1
    );

    // Calculate periodic returns.
    for (std::size_t i = 1;
         i < result.equityCurve.size();
         ++i) {

        const double previousEquity =
            result.equityCurve[i - 1].equity;

        const double currentEquity =
            result.equityCurve[i].equity;

        if (previousEquity > 0.0) {

            const double periodReturn =
                (currentEquity / previousEquity) - 1.0;

            returns.push_back(periodReturn);
        }
    }

    if (returns.size() >= 2) {

        // Calculate mean return.
        double sumReturns = 0.0;

        for (const double value : returns) {
            sumReturns += value;
        }

        const double meanReturn =
            sumReturns /
            static_cast<double>(returns.size());

        // Calculate standard deviation.
        double squaredDifferenceSum = 0.0;

        for (const double value : returns) {

            const double difference =
                value - meanReturn;

            squaredDifferenceSum +=
                difference * difference;
        }

        const double variance =
            squaredDifferenceSum /
            static_cast<double>(returns.size() - 1);

        const double standardDeviation =
            std::sqrt(variance);

        // Sharpe = mean return / return volatility.
        if (standardDeviation > 0.0) {

            result.sharpeRatio =
                meanReturn / standardDeviation;
        }
        else {

            result.sharpeRatio = 0.0;
        }
    }
    else {

        result.sharpeRatio = 0.0;
    }
}
else {

    result.sharpeRatio = 0.0;
}

    return result;
}
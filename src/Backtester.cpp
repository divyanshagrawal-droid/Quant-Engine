#include "../include/Backtester.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

Signal getSignal(
    const std::vector<Candle>& candles,
    StrategyType strategyType,
    std::size_t index,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double rsiBuyThreshold
) {
    if (strategyType == StrategyType::RSI_SMA_TREND) {
        return generateRSISMASignal(
            candles,
            index,
            fastPeriod,
            slowPeriod,
            rsiPeriod,
            rsiBuyThreshold
        );
    }

    return generateSignal(
        candles,
        index,
        fastPeriod,
        slowPeriod
    );
}

BacktestResult runBacktestInternal(
    const std::vector<Candle>& candles,
    StrategyType strategyType,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double rsiBuyThreshold,
    double stopLossPercentage,
    double takeProfitPercentage,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    std::size_t startIndex,
    std::size_t endIndex
) {
    BacktestResult result;
    result.initialCapital = initialCapital;
    result.finalCapital = initialCapital;

    if (candles.empty() || startIndex >= candles.size()) {
        return result;
    }

    endIndex = std::min(endIndex, candles.size() - 1);

    if (startIndex > endIndex) {
        return result;
    }

    if (stopLossPercentage < 0.0 ||
        takeProfitPercentage < 0.0) {
        throw std::invalid_argument(
            "Stop loss and take profit percentages cannot be negative."
        );
    }

    bool inPosition = false;

    double quantity = 0.0;
    double entryPrice = 0.0;
    double entryFee = 0.0;

    double stopLossPrice = 0.0;
    double takeProfitPrice = 0.0;

    std::string entryTime;

    for (std::size_t i = startIndex; i <= endIndex; ++i) {

        Signal signal = Signal::HOLD;

        // Signal is generated from the completed previous candle.
        // Execution happens at the current candle's OPEN.
        if (i > 0) {
            signal = getSignal(
                candles,
                strategyType,
                i - 1,
                fastPeriod,
                slowPeriod,
                rsiPeriod,
                rsiBuyThreshold
            );
        }

        // ============================================================
        // ENTRY
        // ============================================================

        if (signal == Signal::BUY && !inPosition) {

            entryPrice =
                candles[i].open * (1.0 + slippageRate);

            entryTime = candles[i].timestamp;

            quantity =
                result.finalCapital /
                (entryPrice * (1.0 + tradingFeeRate));

            const double entryValue =
                entryPrice * quantity;

            entryFee =
                entryValue * tradingFeeRate;

            // Risk management levels
            stopLossPrice =
                entryPrice * (1.0 - stopLossPercentage);

            takeProfitPrice =
                entryPrice * (1.0 + takeProfitPercentage);

            inPosition = true;
        }

        // ============================================================
        // POSITION MANAGEMENT
        // ============================================================

        else if (inPosition) {

            bool stopLossHit = false;
            bool takeProfitHit = false;

            // Check intrabar risk levels.
            if (candles[i].low <= stopLossPrice) {
                stopLossHit = true;
            }

            if (candles[i].high >= takeProfitPrice) {
                takeProfitHit = true;
            }

            bool exitPosition = false;
            double exitPrice = 0.0;

            // --------------------------------------------------------
            // If both SL and TP are hit in the same candle,
            // use conservative assumption: STOP LOSS happens first.
            // --------------------------------------------------------

            if (stopLossHit) {

                exitPosition = true;

                exitPrice =
                    stopLossPrice * (1.0 - slippageRate);
            }
            else if (takeProfitHit) {

                exitPosition = true;

                exitPrice =
                    takeProfitPrice * (1.0 - slippageRate);
            }
            else if (signal == Signal::SELL) {

                exitPosition = true;

                exitPrice =
                    candles[i].open * (1.0 - slippageRate);
            }

            // --------------------------------------------------------
            // EXIT
            // --------------------------------------------------------

            if (exitPosition) {

                const double exitValue =
                    exitPrice * quantity;

                const double exitFee =
                    exitValue * tradingFeeRate;

                const double grossProfitLoss =
                    (exitPrice - entryPrice) * quantity;

                const double totalFees =
                    entryFee + exitFee;

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

                trade.grossProfitLoss =
                    grossProfitLoss;

                trade.totalFees =
                    totalFees;

                trade.profitLoss =
                    profitLoss;

                result.trades.push_back(trade);

                result.finalCapital += profitLoss;

                inPosition = false;

                quantity = 0.0;
                entryPrice = 0.0;
                entryFee = 0.0;

                stopLossPrice = 0.0;
                takeProfitPrice = 0.0;

                entryTime.clear();
            }
        }

        // ============================================================
        // EQUITY CURVE
        // ============================================================

        double currentEquity =
            result.finalCapital;

        if (inPosition) {

            const double markPrice =
                candles[i].close;

            const double positionValue =
                markPrice * quantity;

            currentEquity =
                result.finalCapital -
                entryPrice * quantity -
                entryFee +
                positionValue;
        }

        result.equityCurve.push_back(
            {candles[i].timestamp, currentEquity}
        );
    }

    // ================================================================
    // FORCE CLOSE AT END OF SELECTED RANGE
    // ================================================================

    if (inPosition) {

        const Candle& finalCandle =
            candles[endIndex];

        const double exitPrice =
            finalCandle.close * (1.0 - slippageRate);

        const double exitValue =
            exitPrice * quantity;

        const double exitFee =
            exitValue * tradingFeeRate;

        const double grossProfitLoss =
            (exitPrice - entryPrice) * quantity;

        const double totalFees =
            entryFee + exitFee;

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

        trade.grossProfitLoss =
            grossProfitLoss;

        trade.totalFees =
            totalFees;

        trade.profitLoss =
            profitLoss;

        result.trades.push_back(trade);

        result.finalCapital += profitLoss;

        inPosition = false;

        if (!result.equityCurve.empty()) {
            result.equityCurve.back().equity =
                result.finalCapital;
        }
    }

    // ================================================================
    // PERFORMANCE METRICS
    // ================================================================

    result.totalProfitLoss =
        result.finalCapital -
        result.initialCapital;

    double peakEquity =
        result.initialCapital;

    double maximumDrawdown = 0.0;
    double maximumDrawdownPercentage = 0.0;

    for (const auto& point : result.equityCurve) {

        if (point.equity > peakEquity) {
            peakEquity = point.equity;
        }

        const double drawdown =
            peakEquity - point.equity;

        maximumDrawdown =
            std::max(
                maximumDrawdown,
                drawdown
            );

        if (peakEquity > 0.0) {

            const double drawdownPercentage =
                (drawdown / peakEquity) * 100.0;

            maximumDrawdownPercentage =
                std::max(
                    maximumDrawdownPercentage,
                    drawdownPercentage
                );
        }
    }

    result.maximumDrawdown =
        maximumDrawdown;

    result.maximumDrawdownPercentage =
        maximumDrawdownPercentage;

    // ================================================================
    // TRADE STATISTICS
    // ================================================================

    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    for (const auto& trade : result.trades) {

        if (trade.profitLoss > 0.0) {

            ++result.winningTrades;

            totalWinningProfit +=
                trade.profitLoss;
        }
        else if (trade.profitLoss < 0.0) {

            ++result.losingTrades;

            totalLosingProfit +=
                trade.profitLoss;
        }
    }

    if (!result.trades.empty()) {

        result.winRate =
            (
                static_cast<double>(
                    result.winningTrades
                )
                /
                static_cast<double>(
                    result.trades.size()
                )
            ) * 100.0;
    }

    if (result.winningTrades > 0) {

        result.averageWin =
            totalWinningProfit /
            static_cast<double>(
                result.winningTrades
            );
    }

    if (result.losingTrades > 0) {

        result.averageLoss =
            totalLosingProfit /
            static_cast<double>(
                result.losingTrades
            );
    }

    if (totalLosingProfit < 0.0) {

        result.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }
    else if (totalWinningProfit > 0.0) {

        result.profitFactor = 999999.0;
    }
    else {

        result.profitFactor = 0.0;
    }

    // ================================================================
    // SHARPE RATIO
    // ================================================================

    if (result.equityCurve.size() >= 2) {

        std::vector<double> returns;

        for (std::size_t i = 1;
             i < result.equityCurve.size();
             ++i) {

            const double previous =
                result.equityCurve[i - 1].equity;

            const double current =
                result.equityCurve[i].equity;

            if (previous > 0.0) {

                returns.push_back(
                    (current / previous) - 1.0
                );
            }
        }

        if (returns.size() >= 2) {

            double sum = 0.0;

            for (double value : returns) {
                sum += value;
            }

            const double mean =
                sum /
                static_cast<double>(
                    returns.size()
                );

            double squaredDifferenceSum = 0.0;

            for (double value : returns) {

                const double difference =
                    value - mean;

                squaredDifferenceSum +=
                    difference * difference;
            }

            const double variance =
                squaredDifferenceSum /
                static_cast<double>(
                    returns.size() - 1
                );

            const double standardDeviation =
                std::sqrt(variance);

            result.sharpeRatio =
                standardDeviation > 0.0
                ? mean / standardDeviation
                : 0.0;
        }
    }

    return result;
}

} // namespace

// ====================================================================
// ORIGINAL SMA BACKTEST
// ====================================================================

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
) {
    if (candles.empty()) {
        return {};
    }

    return runBacktestInternal(
        candles,
        StrategyType::SMA_CROSSOVER,
        fastPeriod,
        slowPeriod,
        14,
        50.0,
        0.0,
        0.0,
        initialCapital,
        tradingFeeRate,
        slippageRate,
        0,
        candles.size() - 1
    );
}

// ====================================================================
// SMA BACKTEST WITH RANGE
// ====================================================================

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    std::size_t startIndex,
    std::size_t endIndex
) {
    return runBacktestInternal(
        candles,
        StrategyType::SMA_CROSSOVER,
        fastPeriod,
        slowPeriod,
        14,
        50.0,
        0.0,
        0.0,
        initialCapital,
        tradingFeeRate,
        slippageRate,
        startIndex,
        endIndex
    );
}

// ====================================================================
// RSI + SMA BACKTEST WITH RISK MANAGEMENT
// ====================================================================

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    StrategyType strategyType,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double rsiBuyThreshold,
    double stopLossPercentage,
    double takeProfitPercentage,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    std::size_t startIndex,
    std::size_t endIndex
) {
    return runBacktestInternal(
        candles,
        strategyType,
        fastPeriod,
        slowPeriod,
        rsiPeriod,
        rsiBuyThreshold,
        stopLossPercentage,
        takeProfitPercentage,
        initialCapital,
        tradingFeeRate,
        slippageRate,
        startIndex,
        endIndex
    );
}
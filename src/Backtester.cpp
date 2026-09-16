#include "../include/Backtester.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

// ============================================================================
// TRUE RANGE
// ============================================================================

double calculateTrueRange(
    const std::vector<Candle>& candles,
    std::size_t index
) {
    if (index == 0) {
        return candles[index].high - candles[index].low;
    }

    const double highLow =
        candles[index].high - candles[index].low;

    const double highPreviousClose =
        std::abs(
            candles[index].high -
            candles[index - 1].close
        );

    const double lowPreviousClose =
        std::abs(
            candles[index].low -
            candles[index - 1].close
        );

    return std::max(
        {
            highLow,
            highPreviousClose,
            lowPreviousClose
        }
    );
}


// ============================================================================
// ATR
// ============================================================================

double calculateATR(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
) {
    if (period == 0 || index + 1 < period) {
        return 0.0;
    }

    double sum = 0.0;

    const std::size_t start =
        index - period + 1;

    for (std::size_t i = start; i <= index; ++i) {
        sum += calculateTrueRange(candles, i);
    }

    return sum /
           static_cast<double>(period);
}


// ============================================================================
// ATR PERCENTAGE
// ============================================================================

double calculateATRPercentage(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period
) {
    if (index >= candles.size()) {
        return 0.0;
    }

    const double close =
        candles[index].close;

    if (close <= 0.0) {
        return 0.0;
    }

    const double atr =
        calculateATR(
            candles,
            index,
            period
        );

    return (atr / close) * 100.0;
}


// ============================================================================
// SIGNAL GENERATOR
// ============================================================================

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


// ============================================================================
// INTERNAL BACKTEST ENGINE
// ============================================================================

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
    std::size_t endIndex,
    std::size_t atrPeriod,
    double maxAtrPercentage
) {
    BacktestResult result;

    result.initialCapital =
        initialCapital;

    result.finalCapital =
        initialCapital;


    // ------------------------------------------------------------------------
    // BASIC VALIDATION
    // ------------------------------------------------------------------------

    if (
        candles.empty() ||
        startIndex >= candles.size()
    ) {
        return result;
    }

    endIndex =
        std::min(
            endIndex,
            candles.size() - 1
        );

    if (startIndex > endIndex) {
        return result;
    }

    if (
        stopLossPercentage < 0.0 ||
        takeProfitPercentage < 0.0
    ) {
        throw std::invalid_argument(
            "Stop loss and take profit percentages cannot be negative."
        );
    }


    // ------------------------------------------------------------------------
    // POSITION STATE
    // ------------------------------------------------------------------------

    bool inPosition = false;

    double quantity = 0.0;

    double entryPrice = 0.0;

    double entryFee = 0.0;

    double stopLossPrice = 0.0;

    double takeProfitPrice = 0.0;

    std::string entryTime;


    // =========================================================================
    // MAIN BACKTEST LOOP
    // =========================================================================

    for (
        std::size_t i = startIndex;
        i <= endIndex;
        ++i
    ) {

        // ---------------------------------------------------------------------
        // SIGNAL
        // ---------------------------------------------------------------------

        Signal signal = Signal::HOLD;

        /*
         * IMPORTANT:
         *
         * Signal is generated from the COMPLETED previous candle.
         * Execution happens at the CURRENT candle OPEN.
         *
         * This prevents look-ahead bias.
         */

        if (i > 0) {

            signal =
                getSignal(
                    candles,
                    strategyType,
                    i - 1,
                    fastPeriod,
                    slowPeriod,
                    rsiPeriod,
                    rsiBuyThreshold
                );
        }


        // =====================================================================
        // VOLATILITY FILTER
        // =====================================================================

        bool volatilityAllowed = true;

        /*
         * maxAtrPercentage <= 0
         * means volatility filter is disabled.
         */

        if (
            maxAtrPercentage > 0.0 &&
            i > 0
        ) {

            /*
             * Use ATR from previous COMPLETED candle.
             *
             * We deliberately do NOT use candles[i].
             * That candle has not completed when we execute
             * at candles[i].open.
             */

            const double previousATRPercentage =
                calculateATRPercentage(
                    candles,
                    i - 1,
                    atrPeriod
                );

            volatilityAllowed =
                previousATRPercentage <=
                maxAtrPercentage;
        }


        // =====================================================================
        // ENTRY
        // =====================================================================

        if (
            signal == Signal::BUY &&
            !inPosition &&
            volatilityAllowed
        ) {

            // Apply positive slippage to BUY
            entryPrice =
                candles[i].open *
                (1.0 + slippageRate);

            entryTime =
                candles[i].timestamp;


            // ---------------------------------------------------------------
            // Position sizing
            // ---------------------------------------------------------------

            quantity =
                result.finalCapital /
                (
                    entryPrice *
                    (1.0 + tradingFeeRate)
                );


            // ---------------------------------------------------------------
            // Entry fee
            // ---------------------------------------------------------------

            const double entryValue =
                entryPrice *
                quantity;

            entryFee =
                entryValue *
                tradingFeeRate;


            // ---------------------------------------------------------------
            // Risk management levels
            // ---------------------------------------------------------------

            stopLossPrice =
                entryPrice *
                (1.0 - stopLossPercentage);

            takeProfitPrice =
                entryPrice *
                (1.0 + takeProfitPercentage);

            inPosition = true;
        }


        // =====================================================================
        // POSITION MANAGEMENT
        // =====================================================================

        else if (inPosition) {

            bool stopLossHit = false;

            bool takeProfitHit = false;


            // ---------------------------------------------------------------
            // Check intrabar stop loss
            // ---------------------------------------------------------------

            if (
                candles[i].low <=
                stopLossPrice
            ) {
                stopLossHit = true;
            }


            // ---------------------------------------------------------------
            // Check intrabar take profit
            // ---------------------------------------------------------------

            if (
                candles[i].high >=
                takeProfitPrice
            ) {
                takeProfitHit = true;
            }


            bool exitPosition = false;

            double exitPrice = 0.0;


            // ----------------------------------------------------------------
            // If both SL and TP are hit in the same candle:
            //
            // Conservative assumption:
            // STOP LOSS happens first.
            // ----------------------------------------------------------------

            if (stopLossHit) {

                exitPosition = true;

                exitPrice =
                    stopLossPrice *
                    (1.0 - slippageRate);
            }

            else if (takeProfitHit) {

                exitPosition = true;

                exitPrice =
                    takeProfitPrice *
                    (1.0 - slippageRate);
            }

            else if (signal == Signal::SELL) {

                exitPosition = true;

                exitPrice =
                    candles[i].open *
                    (1.0 - slippageRate);
            }


            // =================================================================
            // EXIT
            // =================================================================

            if (exitPosition) {

                const double exitValue =
                    exitPrice *
                    quantity;

                const double exitFee =
                    exitValue *
                    tradingFeeRate;

                const double grossProfitLoss =
                    (
                        exitPrice -
                        entryPrice
                    ) *
                    quantity;

                const double totalFees =
                    entryFee +
                    exitFee;

                const double profitLoss =
                    grossProfitLoss -
                    totalFees;


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


                result.trades.push_back(
                    trade
                );


                // Update capital
                result.finalCapital +=
                    profitLoss;


                // Reset position
                inPosition = false;

                quantity = 0.0;

                entryPrice = 0.0;

                entryFee = 0.0;

                stopLossPrice = 0.0;

                takeProfitPrice = 0.0;

                entryTime.clear();
            }
        }


        // =====================================================================
        // EQUITY CURVE
        // =====================================================================

        double currentEquity =
            result.finalCapital;


        if (inPosition) {

            const double markPrice =
                candles[i].close;

            const double positionValue =
                markPrice *
                quantity;


            currentEquity =
                result.finalCapital -
                entryPrice * quantity -
                entryFee +
                positionValue;
        }


        result.equityCurve.push_back(
            {
                candles[i].timestamp,
                currentEquity
            }
        );
    }


    // =========================================================================
    // FORCE CLOSE AT END OF SELECTED RANGE
    // =========================================================================

    if (inPosition) {

        const Candle& finalCandle =
            candles[endIndex];


        const double exitPrice =
            finalCandle.close *
            (1.0 - slippageRate);


        const double exitValue =
            exitPrice *
            quantity;


        const double exitFee =
            exitValue *
            tradingFeeRate;


        const double grossProfitLoss =
            (
                exitPrice -
                entryPrice
            ) *
            quantity;


        const double totalFees =
            entryFee +
            exitFee;


        const double profitLoss =
            grossProfitLoss -
            totalFees;


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


        result.finalCapital +=
            profitLoss;


        inPosition = false;


        // Update final equity point
        if (
            !result.equityCurve.empty()
        ) {

            result.equityCurve.back().equity =
                result.finalCapital;
        }
    }


    // =========================================================================
    // PERFORMANCE METRICS
    // =========================================================================

    result.totalProfitLoss =
        result.finalCapital -
        result.initialCapital;


    // -------------------------------------------------------------------------
    // Maximum Drawdown
    // -------------------------------------------------------------------------

    double peakEquity =
        result.initialCapital;

    double maximumDrawdown = 0.0;

    double maximumDrawdownPercentage = 0.0;


    for (
        const auto& point :
        result.equityCurve
    ) {

        if (
            point.equity >
            peakEquity
        ) {
            peakEquity =
                point.equity;
        }


        const double drawdown =
            peakEquity -
            point.equity;


        maximumDrawdown =
            std::max(
                maximumDrawdown,
                drawdown
            );


        if (peakEquity > 0.0) {

            const double drawdownPercentage =
                (
                    drawdown /
                    peakEquity
                ) *
                100.0;


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


    // =========================================================================
    // TRADE STATISTICS
    // =========================================================================

    double totalWinningProfit = 0.0;

    double totalLosingProfit = 0.0;


    for (
        const auto& trade :
        result.trades
    ) {

        if (
            trade.profitLoss > 0.0
        ) {

            ++result.winningTrades;

            totalWinningProfit +=
                trade.profitLoss;
        }

        else if (
            trade.profitLoss < 0.0
        ) {

            ++result.losingTrades;

            totalLosingProfit +=
                trade.profitLoss;
        }
    }


    // -------------------------------------------------------------------------
    // Win rate
    // -------------------------------------------------------------------------

    if (
        !result.trades.empty()
    ) {

        result.winRate =
            (
                static_cast<double>(
                    result.winningTrades
                )
                /
                static_cast<double>(
                    result.trades.size()
                )
            ) *
            100.0;
    }


    // -------------------------------------------------------------------------
    // Average win
    // -------------------------------------------------------------------------

    if (
        result.winningTrades > 0
    ) {

        result.averageWin =
            totalWinningProfit /
            static_cast<double>(
                result.winningTrades
            );
    }


    // -------------------------------------------------------------------------
    // Average loss
    // -------------------------------------------------------------------------

    if (
        result.losingTrades > 0
    ) {

        result.averageLoss =
            totalLosingProfit /
            static_cast<double>(
                result.losingTrades
            );
    }


    // -------------------------------------------------------------------------
    // Profit factor
    // -------------------------------------------------------------------------

    if (
        totalLosingProfit < 0.0
    ) {

        result.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }

    else if (
        totalWinningProfit > 0.0
    ) {

        result.profitFactor =
            999999.0;
    }

    else {

        result.profitFactor =
            0.0;
    }


    // =========================================================================
    // SHARPE RATIO
    // =========================================================================

    if (
        result.equityCurve.size() >= 2
    ) {

        std::vector<double> returns;


        for (
            std::size_t i = 1;
            i < result.equityCurve.size();
            ++i
        ) {

            const double previous =
                result.equityCurve[i - 1].equity;

            const double current =
                result.equityCurve[i].equity;


            if (previous > 0.0) {

                returns.push_back(
                    (current / previous) -
                    1.0
                );
            }
        }


        if (
            returns.size() >= 2
        ) {

            double sum = 0.0;


            for (
                double value :
                returns
            ) {

                sum += value;
            }


            const double mean =
                sum /
                static_cast<double>(
                    returns.size()
                );


            double squaredDifferenceSum =
                0.0;


            for (
                double value :
                returns
            ) {

                const double difference =
                    value - mean;


                squaredDifferenceSum +=
                    difference *
                    difference;
            }


            const double variance =
                squaredDifferenceSum /
                static_cast<double>(
                    returns.size() - 1
                );


            const double standardDeviation =
                std::sqrt(
                    variance
                );


            result.sharpeRatio =
                standardDeviation > 0.0
                ? mean / standardDeviation
                : 0.0;
        }
    }


    return result;
}

} // namespace


// ============================================================================
// ORIGINAL SMA BACKTEST
// ============================================================================

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
        candles.size() - 1,
        14,
        0.0
    );
}


// ============================================================================
// SMA BACKTEST WITH RANGE
// ============================================================================

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
        endIndex,
        14,
        0.0
    );
}


// ============================================================================
// RSI + SMA BACKTEST WITH RISK MANAGEMENT
// ============================================================================

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
        endIndex,
        14,
        0.0
    );
}


// ============================================================================
// RSI + SMA BACKTEST WITH VOLATILITY FILTER
// ============================================================================

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
    std::size_t endIndex,
    std::size_t atrPeriod,
    double maxAtrPercentage
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
        endIndex,
        atrPeriod,
        maxAtrPercentage
    );
}
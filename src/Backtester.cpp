#include "../include/Backtester.hpp"
#include "../include/Strategy.hpp"

#include <cmath>
#include <vector>

namespace
{
void closePosition(
    BacktestResult& result,
    bool& inPosition,
    double& quantity,
    double& entryPrice,
    double& entryFee,
    std::string& entryTime,
    const Candle& candle,
    double tradingFeeRate,
    double slippageRate,
    bool useOpenPrice)
{
    const double marketPrice =
        useOpenPrice ? candle.open : candle.close;

    const double exitPrice =
        marketPrice * (1.0 - slippageRate);

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
    trade.exitTime = candle.timestamp;
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
}

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate)
{
    if (candles.empty())
    {
        return {};
    }

    return runBacktest(
        candles,
        fastPeriod,
        slowPeriod,
        initialCapital,
        tradingFeeRate,
        slippageRate,
        0,
        candles.size() - 1
    );
}

BacktestResult runBacktest(
    const std::vector<Candle>& candles,
    std::size_t fastPeriod,
    std::size_t slowPeriod,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    std::size_t startIndex,
    std::size_t endIndex)
{
    BacktestResult result;

    result.initialCapital = initialCapital;
    result.finalCapital = initialCapital;

    if (candles.empty() ||
        fastPeriod == 0 ||
        slowPeriod == 0 ||
        fastPeriod >= slowPeriod ||
        slowPeriod > candles.size())
    {
        return result;
    }

    if (startIndex >= candles.size())
    {
        return result;
    }

    if (endIndex >= candles.size())
    {
        endIndex = candles.size() - 1;
    }

    if (startIndex > endIndex)
    {
        return result;
    }

    bool inPosition = false;

    double quantity = 0.0;
    double entryPrice = 0.0;
    double entryFee = 0.0;

    std::string entryTime;

    // ---------------------------------------------------------
    // LOOK-AHEAD-BIAS-SAFE EXECUTION
    //
    // Signal is calculated from the CLOSED candle at i - 1.
    // Execution happens at candle i OPEN.
    // ---------------------------------------------------------
    for (std::size_t i = startIndex;
         i <= endIndex;
         ++i)
    {
        Signal signal = Signal::HOLD;

        if (i > 0)
        {
            signal =
                generateSignal(
                    candles,
                    i - 1,
                    fastPeriod,
                    slowPeriod
                );
        }

        // BUY at next candle OPEN.
        if (signal == Signal::BUY && !inPosition)
        {
            const double marketPrice =
                candles[i].open;

            entryPrice =
                marketPrice * (1.0 + slippageRate);

            entryTime =
                candles[i].timestamp;

            quantity =
                result.finalCapital /
                (entryPrice * (1.0 + tradingFeeRate));

            const double entryValue =
                entryPrice * quantity;

            entryFee =
                entryValue * tradingFeeRate;

            inPosition = true;
        }
        // SELL at next candle OPEN.
        else if (signal == Signal::SELL && inPosition)
        {
            closePosition(
                result,
                inPosition,
                quantity,
                entryPrice,
                entryFee,
                entryTime,
                candles[i],
                tradingFeeRate,
                slippageRate,
                true
            );
        }

        // Mark portfolio to market using the candle CLOSE.
        double currentEquity =
            result.finalCapital;

        if (inPosition)
        {
            const double currentPositionValue =
                candles[i].close * quantity;

            const double entryValue =
                entryPrice * quantity;

            const double cashRemaining =
                result.finalCapital
                - entryValue
                - entryFee;

            currentEquity =
                cashRemaining
                + currentPositionValue;
        }

        EquityPoint point;

        point.timestamp =
            candles[i].timestamp;

        point.equity =
            currentEquity;

        result.equityCurve.push_back(point);

        if (i == endIndex)
        {
            break;
        }
    }

    // ---------------------------------------------------------
    // FORCE CLOSE AT THE END OF THE TEST WINDOW.
    // This uses the final candle CLOSE because there is no
    // future candle available for a next-open execution.
    // ---------------------------------------------------------
    if (inPosition)
    {
        closePosition(
            result,
            inPosition,
            quantity,
            entryPrice,
            entryFee,
            entryTime,
            candles[endIndex],
            tradingFeeRate,
            slippageRate,
            false
        );

        if (!result.equityCurve.empty())
        {
            result.equityCurve.back().equity =
                result.finalCapital;
        }
    }

    result.totalProfitLoss =
        result.finalCapital -
        result.initialCapital;

    // ---------------------------------------------------------
    // MAXIMUM DRAWDOWN
    // ---------------------------------------------------------
    double peakEquity =
        result.initialCapital;

    for (const auto& point : result.equityCurve)
    {
        if (point.equity > peakEquity)
        {
            peakEquity = point.equity;
        }

        const double drawdown =
            peakEquity - point.equity;

        if (drawdown > result.maximumDrawdown)
        {
            result.maximumDrawdown = drawdown;
        }

        if (peakEquity > 0.0)
        {
            const double drawdownPercentage =
                (drawdown / peakEquity) * 100.0;

            if (drawdownPercentage >
                result.maximumDrawdownPercentage)
            {
                result.maximumDrawdownPercentage =
                    drawdownPercentage;
            }
        }
    }

    // ---------------------------------------------------------
    // TRADE STATISTICS
    // ---------------------------------------------------------
    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    for (const auto& trade : result.trades)
    {
        if (trade.profitLoss > 0.0)
        {
            ++result.winningTrades;
            totalWinningProfit += trade.profitLoss;
        }
        else if (trade.profitLoss < 0.0)
        {
            ++result.losingTrades;
            totalLosingProfit += trade.profitLoss;
        }
    }

    if (!result.trades.empty())
    {
        result.winRate =
            (static_cast<double>(result.winningTrades) /
             static_cast<double>(result.trades.size())) *
            100.0;
    }

    if (result.winningTrades > 0)
    {
        result.averageWin =
            totalWinningProfit /
            static_cast<double>(result.winningTrades);
    }

    if (result.losingTrades > 0)
    {
        result.averageLoss =
            totalLosingProfit /
            static_cast<double>(result.losingTrades);
    }

    // ---------------------------------------------------------
    // PROFIT FACTOR
    // ---------------------------------------------------------
    if (totalLosingProfit < 0.0)
    {
        result.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }
    else if (totalWinningProfit > 0.0)
    {
        result.profitFactor = 999999.0;
    }
    else
    {
        result.profitFactor = 0.0;
    }

    // ---------------------------------------------------------
    // PERIODIC SHARPE RATIO
    // ---------------------------------------------------------
    if (result.equityCurve.size() >= 2)
    {
        std::vector<double> returns;

        returns.reserve(
            result.equityCurve.size() - 1
        );

        for (std::size_t i = 1;
             i < result.equityCurve.size();
             ++i)
        {
            const double previousEquity =
                result.equityCurve[i - 1].equity;

            const double currentEquity =
                result.equityCurve[i].equity;

            if (previousEquity > 0.0)
            {
                returns.push_back(
                    (currentEquity / previousEquity) - 1.0
                );
            }
        }

        if (returns.size() >= 2)
        {
            double sumReturns = 0.0;

            for (double value : returns)
            {
                sumReturns += value;
            }

            const double meanReturn =
                sumReturns /
                static_cast<double>(returns.size());

            double squaredDifferenceSum = 0.0;

            for (double value : returns)
            {
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

            if (standardDeviation > 0.0)
            {
                result.sharpeRatio =
                    meanReturn /
                    standardDeviation;
            }
        }
    }

    return result;
}

#include "../include/Optimizer.hpp"
#include "../include/Backtester.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

std::vector<OptimizationResult> optimizeSMA(
    const std::vector<Candle>& candles,
    const std::vector<std::size_t>& fastPeriods,
    const std::vector<std::size_t>& slowPeriods,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
)
{
    std::vector<OptimizationResult> results;

    // =========================================================
    // TEST EVERY FAST / SLOW SMA COMBINATION
    // =========================================================

    for (const std::size_t fastPeriod : fastPeriods)
    {
        for (const std::size_t slowPeriod : slowPeriods)
        {
            // Fast SMA must be smaller than slow SMA.
            if (fastPeriod >= slowPeriod)
            {
                continue;
            }

            // Make sure we have enough candles.
            if (slowPeriod > candles.size())
            {
                continue;
            }

            // =================================================
            // RUN BACKTEST
            // =================================================

            const BacktestResult backtest =
                runBacktest(
                    candles,
                    fastPeriod,
                    slowPeriod,
                    initialCapital,
                    tradingFeeRate,
                    slippageRate
                );

            // =================================================
            // STORE RESULT
            // =================================================

            OptimizationResult optimizationResult;

            optimizationResult.fastPeriod =
                fastPeriod;

            optimizationResult.slowPeriod =
                slowPeriod;

            optimizationResult.totalProfitLoss =
                backtest.totalProfitLoss;

            optimizationResult.finalCapital =
                backtest.finalCapital;

            optimizationResult.maximumDrawdown =
                backtest.maximumDrawdown;

            optimizationResult.maximumDrawdownPercentage =
                backtest.maximumDrawdownPercentage;

            optimizationResult.winRate =
                backtest.winRate;

            optimizationResult.profitFactor =
                backtest.profitFactor;

            optimizationResult.sharpeRatio =
                backtest.sharpeRatio;

            optimizationResult.totalTrades =
                backtest.trades.size();

            results.push_back(
                optimizationResult
            );
        }
    }

    // =========================================================
    // SORT RESULTS
    // =========================================================

    // Primary ranking:
    // Highest Profit Factor first.
    //
    // If Profit Factor is equal:
    // Higher Sharpe Ratio wins.
    //
    // If Sharpe is also equal:
    // Lower Drawdown wins.

    std::sort(
        results.begin(),
        results.end(),
        [](const OptimizationResult& a,
           const OptimizationResult& b)
        {
            if (a.profitFactor != b.profitFactor)
            {
                return a.profitFactor >
                       b.profitFactor;
            }

            if (a.sharpeRatio != b.sharpeRatio)
            {
                return a.sharpeRatio >
                       b.sharpeRatio;
            }

            return a.maximumDrawdownPercentage <
                   b.maximumDrawdownPercentage;
        }
    );

    return results;
}
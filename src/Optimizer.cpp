#include "../include/Optimizer.hpp"
#include "../include/Backtester.hpp"

#include <algorithm>
#include <vector>

std::vector<OptimizationResult> optimizeSMA(
    const std::vector<Candle>& candles,
    const std::vector<std::size_t>& fastPeriods,
    const std::vector<std::size_t>& slowPeriods,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate)
{
    std::vector<OptimizationResult> results;

    // Avoid selecting strategies with almost no observations.
    constexpr std::size_t minimumTrades = 10;

    for (const std::size_t fastPeriod : fastPeriods)
    {
        for (const std::size_t slowPeriod : slowPeriods)
        {
            if (fastPeriod >= slowPeriod)
            {
                continue;
            }

            if (slowPeriod > candles.size())
            {
                continue;
            }

            const BacktestResult backtest =
                runBacktest(
                    candles,
                    fastPeriod,
                    slowPeriod,
                    initialCapital,
                    tradingFeeRate,
                    slippageRate
                );

            if (backtest.trades.size() < minimumTrades)
            {
                continue;
            }

            OptimizationResult result;

            result.fastPeriod = fastPeriod;
            result.slowPeriod = slowPeriod;
            result.totalProfitLoss = backtest.totalProfitLoss;
            result.finalCapital = backtest.finalCapital;
            result.maximumDrawdown = backtest.maximumDrawdown;
            result.maximumDrawdownPercentage =
                backtest.maximumDrawdownPercentage;
            result.winRate = backtest.winRate;
            result.profitFactor = backtest.profitFactor;
            result.sharpeRatio = backtest.sharpeRatio;
            result.totalTrades = backtest.trades.size();

            results.push_back(result);
        }
    }

    // Ranking:
    // 1. Positive P&L beats negative P&L.
    // 2. Higher Profit Factor.
    // 3. Higher Sharpe.
    // 4. Lower drawdown.
    // 5. More trades as a final tie-breaker.
    //
    // This is more robust than ranking by Profit Factor alone.
    std::sort(
        results.begin(),
        results.end(),
        [](const OptimizationResult& a,
           const OptimizationResult& b)
        {
            const bool aProfitable =
                a.totalProfitLoss > 0.0;

            const bool bProfitable =
                b.totalProfitLoss > 0.0;

            if (aProfitable != bProfitable)
            {
                return aProfitable > bProfitable;
            }

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

            if (a.maximumDrawdownPercentage !=
                b.maximumDrawdownPercentage)
            {
                return a.maximumDrawdownPercentage <
                       b.maximumDrawdownPercentage;
            }

            return a.totalTrades >
                   b.totalTrades;
        }
    );

    return results;
}

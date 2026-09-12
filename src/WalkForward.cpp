#include "../include/WalkForward.hpp"
#include "Strategy.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    struct TrainingCandidate
    {
        std::size_t fastPeriod{};
        std::size_t slowPeriod{};
        std::size_t rsiPeriod{};
        double rsiThreshold{};

        BacktestResult result;
    };

    bool isBetterCandidate(
        const TrainingCandidate& a,
        const TrainingCandidate& b
    )
    {
        const bool aProfitable = a.result.totalProfitLoss > 0.0;
        const bool bProfitable = b.result.totalProfitLoss > 0.0;

        if (aProfitable != bProfitable)
            return aProfitable;

        if (a.result.profitFactor != b.result.profitFactor)
            return a.result.profitFactor > b.result.profitFactor;

        if (a.result.sharpeRatio != b.result.sharpeRatio)
            return a.result.sharpeRatio > b.result.sharpeRatio;

        if (a.result.maximumDrawdownPercentage !=
            b.result.maximumDrawdownPercentage)
        {
            return a.result.maximumDrawdownPercentage <
                   b.result.maximumDrawdownPercentage;
        }

        return a.result.trades.size() > b.result.trades.size();
    }
}

std::vector<WalkForwardResult> runWalkForwardValidation(
    const std::vector<Candle>& candles,
    std::size_t trainSize,
    std::size_t testSize,
    const std::vector<std::size_t>& fastPeriods,
    const std::vector<std::size_t>& slowPeriods,
    const std::vector<std::size_t>& rsiPeriods,
    const std::vector<double>& rsiThresholds,
    double stopLossPercentage,
    double takeProfitPercentage,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate
)
{
    std::vector<WalkForwardResult> results;

    if (candles.empty() ||
        trainSize == 0 ||
        testSize == 0 ||
        trainSize + testSize > candles.size())
    {
        return results;
    }

    std::size_t trainStart = 0;

    while (trainStart + trainSize + testSize <= candles.size())
    {
        const std::size_t trainEnd =
            trainStart + trainSize - 1;

        const std::size_t testStart =
            trainEnd + 1;

        const std::size_t testEnd =
            testStart + testSize - 1;

        std::cout << "\n========================================\n";
        std::cout << "WALK-FORWARD WINDOW\n";
        std::cout << "========================================\n";

        std::cout << "Train: "
                  << trainStart << " - "
                  << trainEnd << '\n';

        std::cout << "Test : "
                  << testStart << " - "
                  << testEnd << '\n';

        std::vector<TrainingCandidate> candidates;

        for (std::size_t fast : fastPeriods)
        {
            for (std::size_t slow : slowPeriods)
            {
                if (fast >= slow)
                    continue;

                for (std::size_t rsi : rsiPeriods)
                {
                    for (double threshold : rsiThresholds)
                    {
                        BacktestResult result =
                            runBacktest(
                                candles,
                                StrategyType::RSI_SMA_TREND,
                                fast,
                                slow,
                                rsi,
                                threshold,
                                stopLossPercentage,
                                takeProfitPercentage,
                                initialCapital,
                                tradingFeeRate,
                                slippageRate,
                                trainStart,
                                trainEnd
                            );

                        if (result.trades.size() < 5)
                            continue;

                        TrainingCandidate candidate;

                        candidate.fastPeriod = fast;
                        candidate.slowPeriod = slow;
                        candidate.rsiPeriod = rsi;
                        candidate.rsiThreshold = threshold;
                        candidate.result = result;

                        candidates.push_back(candidate);
                    }
                }
            }
        }

        if (candidates.empty())
        {
            std::cout << "No valid training candidates.\n";

            trainStart += testSize;
            continue;
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            isBetterCandidate
        );

        const TrainingCandidate& best = candidates.front();

        std::cout << "\nBEST TRAINING MODEL\n";
        std::cout << "SMA: "
                  << best.fastPeriod << "/"
                  << best.slowPeriod << '\n';

        std::cout << "RSI: "
                  << best.rsiPeriod << '\n';

        std::cout << "Threshold: "
                  << best.rsiThreshold << '\n';

        std::cout << "Train P&L: "
                  << best.result.totalProfitLoss << '\n';

        std::cout << "Train PF: "
                  << best.result.profitFactor << '\n';

        BacktestResult testResult =
            runBacktest(
                candles,
                StrategyType::RSI_SMA_TREND,
                best.fastPeriod,
                best.slowPeriod,
                best.rsiPeriod,
                best.rsiThreshold,
                stopLossPercentage,
                takeProfitPercentage,
                initialCapital,
                tradingFeeRate,
                slippageRate,
                testStart,
                testEnd
            );

        WalkForwardResult window;

        window.trainStart = trainStart;
        window.trainEnd = trainEnd;
        window.testStart = testStart;
        window.testEnd = testEnd;

        window.fastPeriod = best.fastPeriod;
        window.slowPeriod = best.slowPeriod;
        window.rsiPeriod = best.rsiPeriod;
        window.rsiBuyThreshold = best.rsiThreshold;

        window.testProfitLoss =
            testResult.totalProfitLoss;

        window.testFinalCapital =
            testResult.finalCapital;

        window.testMaximumDrawdownPercentage =
            testResult.maximumDrawdownPercentage;

        window.testWinRate =
            testResult.winRate;

        window.testProfitFactor =
            testResult.profitFactor;

        window.testSharpeRatio =
            testResult.sharpeRatio;

        window.testTrades =
            testResult.trades.size();

        results.push_back(window);

        std::cout << "\nOUT-OF-SAMPLE RESULT\n";
        std::cout << "Test P&L: "
                  << testResult.totalProfitLoss << '\n';

        std::cout << "Test PF: "
                  << testResult.profitFactor << '\n';

        std::cout << "Test DD: "
                  << testResult.maximumDrawdownPercentage << "%\n";

        std::cout << "Test Win Rate: "
                  << testResult.winRate << "%\n";

        std::cout << "Test Trades: "
                  << testResult.trades.size() << '\n';

        trainStart += testSize;
    }

    return results;
}
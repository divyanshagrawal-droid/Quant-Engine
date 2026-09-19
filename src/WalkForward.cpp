#include "../include/WalkForward.hpp"
#include "Strategy.hpp"

#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>

namespace
{
    // ========================================================================
    // TRAINING CANDIDATE
    // ========================================================================

    struct TrainingCandidate
    {
        std::size_t fastPeriod{};
        std::size_t slowPeriod{};
        std::size_t rsiPeriod{};
        double rsiThreshold{};

        BacktestResult result;
    };


    // ========================================================================
    // ATR / TRUE RANGE
    // ========================================================================

    double calculateTrueRange(
        const std::vector<Candle>& candles,
        std::size_t index
    )
    {
        if (index == 0)
        {
            return candles[index].high -
                   candles[index].low;
        }

        const double highLow =
            candles[index].high -
            candles[index].low;

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


    double calculateATRPercentage(
        const std::vector<Candle>& candles,
        std::size_t index,
        std::size_t period
    )
    {
        if (
            period == 0 ||
            index + 1 < period ||
            index >= candles.size()
        )
        {
            return 0.0;
        }

        double trueRangeSum = 0.0;

        const std::size_t start =
            index - period + 1;

        for (
            std::size_t i = start;
            i <= index;
            ++i
        )
        {
            trueRangeSum +=
                calculateTrueRange(
                    candles,
                    i
                );
        }

        const double atr =
            trueRangeSum /
            static_cast<double>(period);

        if (candles[index].close <= 0.0)
        {
            return 0.0;
        }

        return
            (atr / candles[index].close) *
            100.0;
    }


    // ========================================================================
    // PERCENTILE CALCULATION
    // ========================================================================

    double calculatePercentile(
        std::vector<double> values,
        double percentile
    )
    {
        if (values.empty())
        {
            return 0.0;
        }

        if (percentile < 0.0)
        {
            percentile = 0.0;
        }

        if (percentile > 100.0)
        {
            percentile = 100.0;
        }

        std::sort(
            values.begin(),
            values.end()
        );

        const double position =
            (percentile / 100.0) *
            static_cast<double>(
                values.size() - 1
            );

        const std::size_t lowerIndex =
            static_cast<std::size_t>(
                std::floor(position)
            );

        const std::size_t upperIndex =
            static_cast<std::size_t>(
                std::ceil(position)
            );

        if (lowerIndex == upperIndex)
        {
            return values[lowerIndex];
        }

        const double weight =
            position -
            static_cast<double>(lowerIndex);

        return
            values[lowerIndex] *
                (1.0 - weight)
            +
            values[upperIndex] *
                weight;
    }


    // ========================================================================
    // TRAINING-DERIVED VOLATILITY THRESHOLD
    // ========================================================================

    double deriveTrainingVolatilityThreshold(
        const std::vector<Candle>& candles,
        std::size_t trainStart,
        std::size_t trainEnd,
        std::size_t atrPeriod
    )
    {
        std::vector<double> atrPercentages;

        if (
            candles.empty() ||
            trainStart >= candles.size()
        )
        {
            return 0.0;
        }

        trainEnd =
            std::min(
                trainEnd,
                candles.size() - 1
            );

        /*
         * Start after enough candles exist
         * to calculate ATR.
         */

        const std::size_t firstValidIndex =
            std::max(
                trainStart,
                atrPeriod - 1
            );

        if (
            firstValidIndex > trainEnd
        )
        {
            return 0.0;
        }

        for (
            std::size_t i = firstValidIndex;
            i <= trainEnd;
            ++i
        )
        {
            const double atrPercentage =
                calculateATRPercentage(
                    candles,
                    i,
                    atrPeriod
                );

            if (atrPercentage > 0.0)
            {
                atrPercentages.push_back(
                    atrPercentage
                );
            }
        }

        /*
         * Fixed research rule:
         *
         * Use the 75th percentile of ATR%
         * from TRAINING DATA ONLY.
         */

        return calculatePercentile(
            atrPercentages,
            75.0
        );
    }


    // ========================================================================
    // MODEL SELECTION
    // ========================================================================

    bool isBetterCandidate(
        const TrainingCandidate& a,
        const TrainingCandidate& b
    )
    {
        const bool aProfitable =
            a.result.totalProfitLoss > 0.0;

        const bool bProfitable =
            b.result.totalProfitLoss > 0.0;


        if (aProfitable != bProfitable)
        {
            return aProfitable;
        }


        if (
            a.result.profitFactor !=
            b.result.profitFactor
        )
        {
            return
                a.result.profitFactor >
                b.result.profitFactor;
        }


        if (
            a.result.sharpeRatio !=
            b.result.sharpeRatio
        )
        {
            return
                a.result.sharpeRatio >
                b.result.sharpeRatio;
        }


        if (
            a.result.maximumDrawdownPercentage !=
            b.result.maximumDrawdownPercentage
        )
        {
            return
                a.result.maximumDrawdownPercentage <
                b.result.maximumDrawdownPercentage;
        }


        return
            a.result.trades.size() >
            b.result.trades.size();
    }
}


// ============================================================================
// WALK-FORWARD VALIDATION
// ============================================================================

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


    // =========================================================================
    // VALIDATION
    // =========================================================================

    if (
        candles.empty() ||
        trainSize == 0 ||
        testSize == 0 ||
        trainSize + testSize > candles.size()
    )
    {
        return results;
    }


    // =========================================================================
    // VOLATILITY FILTER CONFIGURATION
    // =========================================================================

    /*
     * ATR period is fixed at 14.
     *
     * Volatility threshold is NOT fixed.
     * It is derived from each training window.
     */

    const std::size_t atrPeriod = 14;


    /*
     * Fixed percentile rule.
     *
     * We are NOT optimizing this value on OOS data.
     */

    const double volatilityPercentile = 75.0;


    std::size_t trainStart = 0;


    // =========================================================================
    // WALK-FORWARD LOOP
    // =========================================================================

    while (
        trainStart +
        trainSize +
        testSize <=
        candles.size()
    )
    {
        const std::size_t trainEnd =
            trainStart +
            trainSize -
            1;


        const std::size_t testStart =
            trainEnd + 1;


        const std::size_t testEnd =
            testStart +
            testSize -
            1;


        std::cout
            << "\n========================================\n";

        std::cout
            << "WALK-FORWARD WINDOW\n";

        std::cout
            << "========================================\n";


        std::cout
            << "Train: "
            << trainStart
            << " - "
            << trainEnd
            << '\n';


        std::cout
            << "Test : "
            << testStart
            << " - "
            << testEnd
            << '\n';


        // =====================================================================
        // DERIVE VOLATILITY THRESHOLD FROM TRAINING DATA
        // =====================================================================

        const double maxAtrPercentage =
            deriveTrainingVolatilityThreshold(
                candles,
                trainStart,
                trainEnd,
                atrPeriod
            );


        std::cout
            << "\nTRAINING VOLATILITY MODEL\n";


        std::cout
            << "ATR Period: "
            << atrPeriod
            << '\n';


        std::cout
            << "Percentile: "
            << volatilityPercentile
            << "%\n";


        std::cout
            << "Max ATR%: "
            << maxAtrPercentage
            << "%\n";


        // =====================================================================
        // TRAINING
        // =====================================================================

        std::vector<TrainingCandidate> candidates;


        for (
            std::size_t fast :
            fastPeriods
        )
        {
            for (
                std::size_t slow :
                slowPeriods
            )
            {
                if (fast >= slow)
                {
                    continue;
                }


                for (
                    std::size_t rsi :
                    rsiPeriods
                )
                {
                    for (
                        double threshold :
                        rsiThresholds
                    )
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


                        /*
                         * Ignore candidates with too few trades.
                         */

                        if (
                            result.trades.size() < 5
                        )
                        {
                            continue;
                        }


                        TrainingCandidate candidate;


                        candidate.fastPeriod =
                            fast;

                        candidate.slowPeriod =
                            slow;

                        candidate.rsiPeriod =
                            rsi;

                        candidate.rsiThreshold =
                            threshold;

                        candidate.result =
                            result;


                        candidates.push_back(
                            candidate
                        );
                    }
                }
            }
        }


        // =====================================================================
        // NO VALID CANDIDATE
        // =====================================================================

        if (candidates.empty())
        {
            std::cout
                << "No valid training candidates.\n";


            trainStart +=
                testSize;

            continue;
        }


        // =====================================================================
        // SELECT BEST MODEL
        // =====================================================================

        std::sort(
            candidates.begin(),
            candidates.end(),
            isBetterCandidate
        );


        const TrainingCandidate& best =
            candidates.front();


        std::cout
            << "\nBEST TRAINING MODEL\n";


        std::cout
            << "SMA: "
            << best.fastPeriod
            << "/"
            << best.slowPeriod
            << '\n';


        std::cout
            << "RSI: "
            << best.rsiPeriod
            << '\n';


        std::cout
            << "Threshold: "
            << best.rsiThreshold
            << '\n';


        std::cout
            << "Train P&L: "
            << best.result.totalProfitLoss
            << '\n';


        std::cout
            << "Train PF: "
            << best.result.profitFactor
            << '\n';


        // =====================================================================
        // OUT-OF-SAMPLE TEST
        // =====================================================================

        /*
         * IMPORTANT:
         *
         * maxAtrPercentage was calculated exclusively
         * from the training window.
         *
         * The test window is never used to derive it.
         */

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
                testEnd,
                atrPeriod,
                maxAtrPercentage
            );


        // =====================================================================
        // STORE WALK-FORWARD RESULT
        // =====================================================================

        WalkForwardResult window;


        window.trainStart =
            trainStart;

        window.trainEnd =
            trainEnd;

        window.testStart =
            testStart;

        window.testEnd =
            testEnd;


        window.fastPeriod =
            best.fastPeriod;

        window.slowPeriod =
            best.slowPeriod;

        window.rsiPeriod =
            best.rsiPeriod;

        window.rsiBuyThreshold =
            best.rsiThreshold;


        // Volatility model

        window.atrPeriod =
            atrPeriod;

        window.maxAtrPercentage =
            maxAtrPercentage;


        // OOS metrics

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


        // =====================================================================
        // STORE OOS EQUITY CURVE
        // =====================================================================

        window.testEquityCurve =
            testResult.equityCurve;


        results.push_back(
            window
        );


        // =====================================================================
        // DISPLAY OOS RESULT
        // =====================================================================

        std::cout
            << "\nOUT-OF-SAMPLE RESULT\n";


        std::cout
            << "Test P&L: "
            << testResult.totalProfitLoss
            << '\n';


        std::cout
            << "Test PF: "
            << testResult.profitFactor
            << '\n';


        std::cout
            << "Test DD: "
            << testResult.maximumDrawdownPercentage
            << "%\n";


        std::cout
            << "Test Win Rate: "
            << testResult.winRate
            << "%\n";


        std::cout
            << "Test Trades: "
            << testResult.trades.size()
            << '\n';


        // =====================================================================
        // MOVE TO NEXT WINDOW
        // =====================================================================

        trainStart +=
            testSize;
    }


    return results;
}
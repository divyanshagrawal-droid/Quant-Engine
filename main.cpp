#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"
#include "include/WalkForward.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct OOSMetrics
{
    double finalCapital{};
    double profitLoss{};
    double returnPercentage{};

    double maximumDrawdown{};
    double maximumDrawdownPercentage{};

    double sharpeRatio{};
    double winRate{};
    double expectancy{};
    double profitFactor{};

    std::size_t totalTrades{};
    std::size_t winningTrades{};
    std::size_t losingTrades{};

    double averageWin{};
    double averageLoss{};

    std::vector<EquityPoint> equityCurve;
};


// =========================================================
// CALCULATE MAXIMUM DRAWDOWN
// =========================================================

void calculateDrawdown(
    const std::vector<EquityPoint>& equityCurve,
    double initialCapital,
    double& maximumDrawdown,
    double& maximumDrawdownPercentage)
{
    double peak = initialCapital;

    maximumDrawdown = 0.0;
    maximumDrawdownPercentage = 0.0;

    for (const auto& point : equityCurve)
    {
        if (point.equity > peak)
        {
            peak = point.equity;
        }

        const double drawdown =
            peak - point.equity;

        if (drawdown > maximumDrawdown)
        {
            maximumDrawdown = drawdown;
        }

        if (peak > 0.0)
        {
            const double drawdownPercentage =
                (drawdown / peak) * 100.0;

            if (drawdownPercentage >
                maximumDrawdownPercentage)
            {
                maximumDrawdownPercentage =
                    drawdownPercentage;
            }
        }
    }
}


// =========================================================
// CALCULATE SHARPE
// =========================================================

double calculateSharpe(
    const std::vector<EquityPoint>& equityCurve)
{
    if (equityCurve.size() < 2)
    {
        return 0.0;
    }

    std::vector<double> returns;

    for (std::size_t i = 1;
         i < equityCurve.size();
         ++i)
    {
        const double previous =
            equityCurve[i - 1].equity;

        const double current =
            equityCurve[i].equity;

        if (previous > 0.0)
        {
            returns.push_back(
                (current / previous) - 1.0
            );
        }
    }

    if (returns.size() < 2)
    {
        return 0.0;
    }

    double sum = 0.0;

    for (double value : returns)
    {
        sum += value;
    }

    const double mean =
        sum / static_cast<double>(returns.size());

    double squaredDifferenceSum = 0.0;

    for (double value : returns)
    {
        const double difference =
            value - mean;

        squaredDifferenceSum +=
            difference * difference;
    }

    const double variance =
        squaredDifferenceSum /
        static_cast<double>(returns.size() - 1);

    const double standardDeviation =
        std::sqrt(variance);

    if (standardDeviation <= 0.0)
    {
        return 0.0;
    }

    return mean / standardDeviation;
}


// =========================================================
// BUILD COMPOUNDED FILTERED OOS RESULTS
// =========================================================

OOSMetrics calculateFilteredOOSMetrics(
    const std::vector<Candle>& candles,
    const std::vector<WalkForwardResult>& results,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    double stopLossPercentage,
    double takeProfitPercentage)
{
    OOSMetrics output;

    if (results.empty())
    {
        return output;
    }

    double combinedCapital =
        initialCapital;

    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    std::size_t winningTrades = 0;
    std::size_t losingTrades = 0;

    // Explicit starting point.
    output.equityCurve.push_back(
        {
            candles[results.front().testStart].timestamp,
            initialCapital
        }
    );

    // =====================================================
    // REPLAY EVERY OOS WINDOW
    // =====================================================

    for (const auto& window : results)
    {
        const BacktestResult testResult =
            runBacktest(
                candles,
                StrategyType::RSI_SMA_TREND,

                window.fastPeriod,
                window.slowPeriod,
                window.rsiPeriod,
                window.rsiBuyThreshold,

                stopLossPercentage,
                takeProfitPercentage,

                initialCapital,
                tradingFeeRate,
                slippageRate,

                window.testStart,
                window.testEnd,

                window.atrPeriod,
                window.maxAtrPercentage
            );

        // =================================================
        // TRADE STATISTICS
        // =================================================

        for (const auto& trade : testResult.trades)
        {
            if (trade.profitLoss > 0.0)
            {
                totalWinningProfit +=
                    trade.profitLoss;

                ++winningTrades;
            }
            else if (trade.profitLoss < 0.0)
            {
                totalLosingProfit +=
                    trade.profitLoss;

                ++losingTrades;
            }
        }

        // =================================================
        // STITCH EQUITY CURVES
        // =================================================

        for (const auto& point :
             testResult.equityCurve)
        {
            const double normalizedEquity =
                point.equity / initialCapital;

            const double combinedEquity =
                combinedCapital *
                normalizedEquity;

            output.equityCurve.push_back(
                {
                    point.timestamp,
                    combinedEquity
                }
            );
        }

        // =================================================
        // COMPOUND CAPITAL INTO NEXT WINDOW
        // =================================================

        combinedCapital =
            combinedCapital *
            (
                testResult.finalCapital /
                initialCapital
            );
    }

    // =====================================================
    // FINAL CAPITAL
    // =====================================================

    output.finalCapital =
        combinedCapital;

    output.profitLoss =
        combinedCapital -
        initialCapital;

    output.returnPercentage =
        (
            (combinedCapital / initialCapital)
            - 1.0
        ) * 100.0;

    // =====================================================
    // TRADE METRICS
    // =====================================================

    output.winningTrades =
        winningTrades;

    output.losingTrades =
        losingTrades;

    output.totalTrades =
        winningTrades +
        losingTrades;

    if (output.totalTrades > 0)
    {
        output.winRate =
            (
                static_cast<double>(winningTrades) /
                static_cast<double>(output.totalTrades)
            ) * 100.0;
    }

    if (winningTrades > 0)
    {
        output.averageWin =
            totalWinningProfit /
            static_cast<double>(winningTrades);
    }

    if (losingTrades > 0)
    {
        output.averageLoss =
            totalLosingProfit /
            static_cast<double>(losingTrades);
    }

    const double winProbability =
        output.totalTrades > 0
            ? static_cast<double>(winningTrades) /
              static_cast<double>(output.totalTrades)
            : 0.0;

    const double lossProbability =
        output.totalTrades > 0
            ? static_cast<double>(losingTrades) /
              static_cast<double>(output.totalTrades)
            : 0.0;

    output.expectancy =
        (
            winProbability *
            output.averageWin
        )
        +
        (
            lossProbability *
            output.averageLoss
        );

    // =====================================================
    // PROFIT FACTOR
    // =====================================================

    if (totalLosingProfit < 0.0)
    {
        output.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }
    else if (totalWinningProfit > 0.0)
    {
        output.profitFactor =
            999999.0;
    }
    else
    {
        output.profitFactor =
            0.0;
    }

    // =====================================================
    // DRAWDOWN
    // =====================================================

    calculateDrawdown(
        output.equityCurve,
        initialCapital,
        output.maximumDrawdown,
        output.maximumDrawdownPercentage
    );

    // =====================================================
    // SHARPE
    // =====================================================

    output.sharpeRatio =
        calculateSharpe(
            output.equityCurve
        );

    return output;
}


// =========================================================
// SAVE EQUITY CURVE
// =========================================================

void saveEquityCurve(
    const std::string& filename,
    const std::vector<EquityPoint>& equityCurve)
{
    std::ofstream outputFile(filename);

    if (!outputFile)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << "\n";

        return;
    }

    outputFile
        << "timestamp,equity\n";

    outputFile
        << std::fixed
        << std::setprecision(8);

    for (const auto& point :
         equityCurve)
    {
        outputFile
            << point.timestamp
            << ","
            << point.equity
            << "\n";
    }

    outputFile.close();

    std::cout
        << "\nEquity curve saved to:\n"
        << filename
        << "\n";
}


// =========================================================
// SAVE WINDOW RESULTS
// =========================================================

void saveWindowResults(
    const std::string& filename,
    const std::vector<WalkForwardResult>& results)
{
    std::ofstream outputFile(filename);

    if (!outputFile)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << "\n";

        return;
    }

    outputFile
        << "window,train_start,train_end,"
        << "test_start,test_end,"
        << "fast_period,slow_period,"
        << "rsi_period,rsi_threshold,"
        << "atr_period,max_atr_percentage,"
        << "test_profit_loss,test_final_capital,"
        << "test_dd_percentage,test_win_rate,"
        << "test_profit_factor,test_sharpe,"
        << "test_trades\n";

    outputFile
        << std::fixed
        << std::setprecision(8);

    for (std::size_t i = 0;
         i < results.size();
         ++i)
    {
        const auto& result =
            results[i];

        outputFile
            << (i + 1)
            << ","
            << result.trainStart
            << ","
            << result.trainEnd
            << ","
            << result.testStart
            << ","
            << result.testEnd
            << ","
            << result.fastPeriod
            << ","
            << result.slowPeriod
            << ","
            << result.rsiPeriod
            << ","
            << result.rsiBuyThreshold
            << ","
            << result.atrPeriod
            << ","
            << result.maxAtrPercentage
            << ","
            << result.testProfitLoss
            << ","
            << result.testFinalCapital
            << ","
            << result.testMaximumDrawdownPercentage
            << ","
            << result.testWinRate
            << ","
            << result.testProfitFactor
            << ","
            << result.testSharpeRatio
            << ","
            << result.testTrades
            << "\n";
    }

    outputFile.close();

    std::cout
        << "Window results saved to:\n"
        << filename
        << "\n";
}


// =========================================================
// PRINT OOS SUMMARY
// =========================================================

void printOOSSummary(
    const OOSMetrics& metrics)
{
    std::cout
        << "\n========================================\n"
        << "       FILTERED OOS SUMMARY\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Initial Capital : "
        << 100000.0
        << "\n";

    std::cout
        << "Final Capital   : "
        << metrics.finalCapital
        << "\n";

    std::cout
        << "Total P&L       : "
        << metrics.profitLoss
        << "\n";

    std::cout
        << "Return          : "
        << metrics.returnPercentage
        << "%\n";

    std::cout
        << "Max Drawdown    : "
        << metrics.maximumDrawdown
        << "\n";

    std::cout
        << "Max DD %        : "
        << metrics.maximumDrawdownPercentage
        << "%\n";

    std::cout
        << "Sharpe          : "
        << metrics.sharpeRatio
        << "\n";

    std::cout
        << "Profit Factor   : "
        << metrics.profitFactor
        << "\n";

    std::cout
        << "Win Rate        : "
        << metrics.winRate
        << "%\n";

    std::cout
        << "Expectancy      : "
        << metrics.expectancy
        << "\n";

    std::cout
        << "Average Win     : "
        << metrics.averageWin
        << "\n";

    std::cout
        << "Average Loss    : "
        << metrics.averageLoss
        << "\n";

    std::cout
        << "Total Trades    : "
        << metrics.totalTrades
        << "\n";

    std::cout
        << "Winning Trades  : "
        << metrics.winningTrades
        << "\n";

    std::cout
        << "Losing Trades   : "
        << metrics.losingTrades
        << "\n";
}


// =========================================================
// MAIN
// =========================================================

int main()
{
    const std::string dataFile =
        "data/BTCUSDT.csv";

    const double initialCapital =
        100000.0;

    const double stopLossPercentage =
        0.02;

    const double takeProfitPercentage =
        0.04;

    // =========================================================
    // LOAD DATA
    // =========================================================

    const std::vector<Candle> candles =
        readCSV(dataFile);

    if (candles.empty())
    {
        std::cerr
            << "Error: No candles loaded.\n";

        return 1;
    }

    std::cout
        << "\nCandles loaded: "
        << candles.size()
        << "\n";

    // =========================================================
    // BASELINE COSTS
    // =========================================================

    const double baselineFee =
        0.001;

    const double baselineSlippage =
        0.0005;

    // =========================================================
    // WALK-FORWARD CONFIGURATION
    // =========================================================

    const std::size_t trainSize =
        4320;

    const std::size_t testSize =
        720;

    const std::vector<std::size_t>
        fastPeriods =
        {
            3, 5, 8
        };

    const std::vector<std::size_t>
        slowPeriods =
        {
            10, 15, 20, 30
        };

    const std::vector<std::size_t>
        rsiPeriods =
        {
            7, 14, 21
        };

    const std::vector<double>
        rsiThresholds =
        {
            50.0, 55.0, 60.0
        };

    // =========================================================
    // VOLATILITY-FILTERED WALK-FORWARD
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "   VOLATILITY-FILTERED WALK-FORWARD\n"
        << "========================================\n";

    std::cout
        << "Fee       : "
        << baselineFee * 100.0
        << "%\n";

    std::cout
        << "Slippage  : "
        << baselineSlippage * 100.0
        << "%\n";

    std::cout
        << "Train Size: "
        << trainSize
        << "\n";

    std::cout
        << "Test Size : "
        << testSize
        << "\n";

    const std::vector<WalkForwardResult>
        filteredResults =
        runWalkForwardValidation(
            candles,
            trainSize,
            testSize,
            fastPeriods,
            slowPeriods,
            rsiPeriods,
            rsiThresholds,
            stopLossPercentage,
            takeProfitPercentage,
            initialCapital,
            baselineFee,
            baselineSlippage
        );

    if (filteredResults.empty())
    {
        std::cerr
            << "\nError: No walk-forward results.\n";

        return 1;
    }

    // =========================================================
    // SELECTED OOS PARAMETERS
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       SELECTED OOS PARAMETERS\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(6);

    for (std::size_t i = 0;
         i < filteredResults.size();
         ++i)
    {
        const auto& result =
            filteredResults[i];

        std::cout
            << "\nWindow "
            << (i + 1)
            << "\n";

        std::cout
            << "SMA           : "
            << result.fastPeriod
            << "/"
            << result.slowPeriod
            << "\n";

        std::cout
            << "RSI Period    : "
            << result.rsiPeriod
            << "\n";

        std::cout
            << "RSI Threshold : "
            << result.rsiBuyThreshold
            << "\n";

        std::cout
            << "ATR Period    : "
            << result.atrPeriod
            << "\n";

        std::cout
            << "Max ATR %     : "
            << result.maxAtrPercentage
            << "%\n";

        std::cout
            << "OOS P&L       : "
            << result.testProfitLoss
            << "\n";

        std::cout
            << "OOS Final     : "
            << result.testFinalCapital
            << "\n";

        std::cout
            << "OOS DD %      : "
            << result.testMaximumDrawdownPercentage
            << "%\n";

        std::cout
            << "OOS PF        : "
            << result.testProfitFactor
            << "\n";

        std::cout
            << "OOS Trades    : "
            << result.testTrades
            << "\n";
    }

    // =========================================================
    // COMBINED FILTERED OOS METRICS
    // =========================================================

    const OOSMetrics filteredMetrics =
        calculateFilteredOOSMetrics(
            candles,
            filteredResults,
            initialCapital,
            baselineFee,
            baselineSlippage,
            stopLossPercentage,
            takeProfitPercentage
        );

    printOOSSummary(
        filteredMetrics
    );

    // =========================================================
    // SAVE FILTERED OOS EQUITY CURVE
    // =========================================================

    saveEquityCurve(
        "results/volatility_filtered_oos_equity.csv",
        filteredMetrics.equityCurve
    );

    // =========================================================
    // SAVE WINDOW RESULTS
    // =========================================================

    saveWindowResults(
        "results/volatility_filtered_wfv.csv",
        filteredResults
    );

    // =========================================================
    // PRINT WINDOW SUMMARY TABLE
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       FILTERED OOS WINDOW SUMMARY\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\nWindow   P&L        Return      DD%      PF      Trades\n";

    std::cout
        << "----------------------------------------------------------\n";

    for (std::size_t i = 0;
         i < filteredResults.size();
         ++i)
    {
        const auto& result =
            filteredResults[i];

        const double windowReturn =
            (
                (result.testFinalCapital /
                 initialCapital)
                - 1.0
            ) * 100.0;

        std::cout
            << std::setw(4)
            << (i + 1)

            << std::setw(12)
            << result.testProfitLoss

            << std::setw(12)
            << windowReturn

            << std::setw(10)
            << result.testMaximumDrawdownPercentage

            << std::setw(9)
            << result.testProfitFactor

            << std::setw(10)
            << result.testTrades

            << "\n";
    }

    // =========================================================
    // FILTER STATUS
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          FILTER STATUS\n"
        << "========================================\n";

    std::cout
        << "ATR-based volatility filter: ENABLED\n";

    std::cout
        << "ATR period: 14\n";

    std::cout
        << "Threshold: training 75th percentile\n";

    std::cout
        << "Threshold source: training data only\n";

    std::cout
        << "Look-ahead protection: ENABLED\n";

    // =========================================================
    // BASIC INTERPRETATION
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "        FILTERED OOS INTERPRETATION\n"
        << "========================================\n";

    if (filteredMetrics.returnPercentage > 0.0)
    {
        std::cout
            << "OOS Return: POSITIVE\n";
    }
    else
    {
        std::cout
            << "OOS Return: NEGATIVE\n";
    }

    if (filteredMetrics.profitFactor > 1.0)
    {
        std::cout
            << "Profit Factor: ABOVE 1\n";
    }
    else
    {
        std::cout
            << "Profit Factor: BELOW 1\n";
    }

    if (filteredMetrics.expectancy > 0.0)
    {
        std::cout
            << "Expectancy: POSITIVE\n";
    }
    else
    {
        std::cout
            << "Expectancy: NEGATIVE\n";
    }

    // =========================================================
    // DONE
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "                 DONE\n"
        << "========================================\n";

    return 0;
}
#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

struct RobustnessResult
{
    std::size_t fastPeriod{};
    std::size_t slowPeriod{};
    std::size_t rsiPeriod{};
    double threshold{};

    double finalCapital{};
    double returnPercentage{};
    double profitFactor{};
    double expectancy{};
    double maximumDrawdownPercentage{};

    std::size_t totalTrades{};
};


// =========================================================
// CALCULATE MAXIMUM DRAWDOWN
// =========================================================

double calculateMaximumDrawdown(
    const std::vector<EquityPoint>& curve,
    double initialCapital)
{
    double peak = initialCapital;
    double maximumDrawdownPercentage = 0.0;

    for (const auto& point : curve)
    {
        if (point.equity > peak)
        {
            peak = point.equity;
        }

        if (peak > 0.0)
        {
            const double drawdownPercentage =
                ((peak - point.equity) / peak) * 100.0;

            if (drawdownPercentage >
                maximumDrawdownPercentage)
            {
                maximumDrawdownPercentage =
                    drawdownPercentage;
            }
        }
    }

    return maximumDrawdownPercentage;
}


// =========================================================
// TEST ONE FIXED PARAMETER SET ACROSS ALL OOS WINDOWS
// =========================================================

RobustnessResult evaluateParameters(
    const std::vector<Candle>& candles,
    const std::vector<std::pair<std::size_t, std::size_t>>&
        oosWindows,

    std::size_t fastPeriod,
    std::size_t slowPeriod,
    std::size_t rsiPeriod,
    double threshold,

    double initialCapital,
    double fee,
    double slippage,
    double stopLossPercentage,
    double takeProfitPercentage)
{
    RobustnessResult result;

    result.fastPeriod = fastPeriod;
    result.slowPeriod = slowPeriod;
    result.rsiPeriod = rsiPeriod;
    result.threshold = threshold;

    double combinedCapital =
        initialCapital;

    std::vector<EquityPoint> combinedCurve;

    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    std::size_t winningTrades = 0;
    std::size_t losingTrades = 0;

    for (const auto& window : oosWindows)
    {
        const std::size_t testStart =
            window.first;

        const std::size_t testEnd =
            window.second;

        const BacktestResult testResult =
            runBacktest(
                candles,
                StrategyType::RSI_SMA_TREND,

                fastPeriod,
                slowPeriod,
                rsiPeriod,
                threshold,

                stopLossPercentage,
                takeProfitPercentage,

                initialCapital,
                fee,
                slippage,

                testStart,
                testEnd
            );

        // -----------------------------------------------------
        // Aggregate trades
        // -----------------------------------------------------

        for (const auto& trade :
             testResult.trades)
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

        // -----------------------------------------------------
        // Build compounded equity curve
        // -----------------------------------------------------

        for (const auto& point :
             testResult.equityCurve)
        {
            const double normalizedEquity =
                point.equity / initialCapital;

            const double combinedEquity =
                combinedCapital *
                normalizedEquity;

            combinedCurve.push_back(
                {
                    point.timestamp,
                    combinedEquity
                }
            );
        }

        // -----------------------------------------------------
        // Compound into next OOS window
        // -----------------------------------------------------

        combinedCapital =
            combinedCapital *
            (
                testResult.finalCapital /
                initialCapital
            );
    }

    // =========================================================
    // FINAL CAPITAL
    // =========================================================

    result.finalCapital =
        combinedCapital;

    result.returnPercentage =
        (
            (combinedCapital / initialCapital)
            - 1.0
        ) * 100.0;

    // =========================================================
    // TRADES
    // =========================================================

    result.totalTrades =
        winningTrades +
        losingTrades;

    // =========================================================
    // EXPECTANCY
    // =========================================================

    double averageWin = 0.0;
    double averageLoss = 0.0;

    if (winningTrades > 0)
    {
        averageWin =
            totalWinningProfit /
            static_cast<double>(winningTrades);
    }

    if (losingTrades > 0)
    {
        averageLoss =
            totalLosingProfit /
            static_cast<double>(losingTrades);
    }

    double winRate = 0.0;

    if (result.totalTrades > 0)
    {
        winRate =
            static_cast<double>(winningTrades) /
            static_cast<double>(result.totalTrades);
    }

    const double lossRate =
        1.0 - winRate;

    result.expectancy =
        (winRate * averageWin)
        +
        (lossRate * averageLoss);

    // =========================================================
    // PROFIT FACTOR
    // =========================================================

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

    // =========================================================
    // MAXIMUM DRAWDOWN
    // =========================================================

    result.maximumDrawdownPercentage =
        calculateMaximumDrawdown(
            combinedCurve,
            initialCapital
        );

    return result;
}


// =========================================================
// SAVE SMA ROBUSTNESS CSV
// =========================================================

void saveSMACSV(
    const std::vector<RobustnessResult>& results)
{
    std::ofstream file(
        "results/sma_parameter_robustness.csv"
    );

    file
        << "fast_period,slow_period,"
        << "rsi_period,threshold,"
        << "final_capital,return_percentage,"
        << "profit_factor,expectancy,"
        << "maximum_drawdown_percentage,"
        << "total_trades\n";

    for (const auto& result : results)
    {
        file
            << result.fastPeriod << ","
            << result.slowPeriod << ","
            << result.rsiPeriod << ","
            << result.threshold << ","
            << result.finalCapital << ","
            << result.returnPercentage << ","
            << result.profitFactor << ","
            << result.expectancy << ","
            << result.maximumDrawdownPercentage << ","
            << result.totalTrades
            << '\n';
    }

    file.close();
}


// =========================================================
// SAVE RSI ROBUSTNESS CSV
// =========================================================

void saveRSICSV(
    const std::vector<RobustnessResult>& results)
{
    std::ofstream file(
        "results/rsi_parameter_robustness.csv"
    );

    file
        << "fast_period,slow_period,"
        << "rsi_period,threshold,"
        << "final_capital,return_percentage,"
        << "profit_factor,expectancy,"
        << "maximum_drawdown_percentage,"
        << "total_trades\n";

    for (const auto& result : results)
    {
        file
            << result.fastPeriod << ","
            << result.slowPeriod << ","
            << result.rsiPeriod << ","
            << result.threshold << ","
            << result.finalCapital << ","
            << result.returnPercentage << ","
            << result.profitFactor << ","
            << result.expectancy << ","
            << result.maximumDrawdownPercentage << ","
            << result.totalTrades
            << '\n';
    }

    file.close();
}


// =========================================================
// MAIN
// =========================================================

int main()
{
    // =========================================================
    // CONFIGURATION
    // =========================================================

    const std::string dataFile =
        "data/BTCUSDT.csv";

    const double initialCapital =
        100000.0;

    const double fee =
        0.001;

    const double slippage =
        0.0005;

    const double stopLossPercentage =
        0.02;

    const double takeProfitPercentage =
        0.04;

    // Same OOS windows used by our WFV.
    const std::vector<std::pair<std::size_t, std::size_t>>
        oosWindows =
    {
        {4320, 5039},
        {5040, 5759},
        {5760, 6479},
        {6480, 7199},
        {7200, 7919},
        {7920, 8639}
    };

    const std::vector<std::size_t> fastPeriods =
    {
        2, 3, 4, 5, 6, 8, 10
    };

    const std::vector<std::size_t> slowPeriods =
    {
        15, 20, 25, 30, 40, 50
    };

    const std::vector<std::size_t> rsiPeriods =
    {
        7, 14, 21, 28
    };

    const std::vector<double> thresholds =
    {
        45, 50, 55, 60, 65
    };

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
        << "\n========================================\n"
        << "      PARAMETER ROBUSTNESS ANALYSIS\n"
        << "========================================\n";

    std::cout
        << "OOS Windows : "
        << oosWindows.size()
        << '\n';

    std::cout
        << "Fee         : "
        << fee * 100.0
        << "%\n";

    std::cout
        << "Slippage    : "
        << slippage * 100.0
        << "%\n";

    // =========================================================
    // SMA ROBUSTNESS
    //
    // RSI fixed at 21
    // Threshold fixed at 60
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          SMA ROBUSTNESS MAP\n"
        << "========================================\n";

    std::vector<RobustnessResult> smaResults;

    for (const auto fast : fastPeriods)
    {
        for (const auto slow : slowPeriods)
        {
            if (fast >= slow)
            {
                continue;
            }

            const RobustnessResult result =
                evaluateParameters(
                    candles,
                    oosWindows,

                    fast,
                    slow,
                    21,
                    60.0,

                    initialCapital,
                    fee,
                    slippage,
                    stopLossPercentage,
                    takeProfitPercentage
                );

            smaResults.push_back(result);
        }
    }

    std::cout
        << "\nFast  Slow    Return      PF"
        << "      Expectancy      DD%      Trades\n";

    std::cout
        << "------------------------------------------------\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    for (const auto& result :
         smaResults)
    {
        std::cout
            << std::setw(4)
            << result.fastPeriod
            << "  "

            << std::setw(4)
            << result.slowPeriod
            << "  "

            << std::setw(9)
            << result.returnPercentage
            << "%  "

            << std::setw(7)
            << result.profitFactor
            << "  "

            << std::setw(13)
            << result.expectancy
            << "  "

            << std::setw(8)
            << result.maximumDrawdownPercentage
            << "  "

            << std::setw(6)
            << result.totalTrades

            << '\n';
    }

    saveSMACSV(smaResults);

    // =========================================================
    // RSI ROBUSTNESS
    //
    // SMA fixed at 3/20
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          RSI ROBUSTNESS MAP\n"
        << "========================================\n";

    std::vector<RobustnessResult> rsiResults;

    for (const auto rsi : rsiPeriods)
    {
        for (const auto threshold : thresholds)
        {
            const RobustnessResult result =
                evaluateParameters(
                    candles,
                    oosWindows,

                    3,
                    20,
                    rsi,
                    threshold,

                    initialCapital,
                    fee,
                    slippage,
                    stopLossPercentage,
                    takeProfitPercentage
                );

            rsiResults.push_back(result);
        }
    }

    std::cout
        << "\nRSI  Threshold    Return      PF"
        << "      Expectancy      DD%      Trades\n";

    std::cout
        << "------------------------------------------------\n";

    for (const auto& result :
         rsiResults)
    {
        std::cout
            << std::setw(3)
            << result.rsiPeriod
            << "     "

            << std::setw(5)
            << result.threshold
            << "      "

            << std::setw(8)
            << result.returnPercentage
            << "%  "

            << std::setw(7)
            << result.profitFactor
            << "  "

            << std::setw(13)
            << result.expectancy
            << "  "

            << std::setw(8)
            << result.maximumDrawdownPercentage
            << "  "

            << std::setw(6)
            << result.totalTrades

            << '\n';
    }

    saveRSICSV(rsiResults);

    // =========================================================
    // FILE OUTPUT
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "              FILES SAVED\n"
        << "========================================\n";

    std::cout
        << "results/sma_parameter_robustness.csv\n";

    std::cout
        << "results/rsi_parameter_robustness.csv\n";

    // =========================================================
    // RESEARCH WARNING
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          IMPORTANT NOTE\n"
        << "========================================\n";

    std::cout
        << "These OOS parameter maps are diagnostic.\n";

    std::cout
        << "Do NOT select the best parameter from\n"
        << "these OOS results and claim it as a\n"
        << "new unbiased OOS result.\n";

    std::cout
        << "\nThe goal is to determine whether a\n"
        << "stable parameter region exists.\n";

    std::cout
        << "\n========================================\n"
        << "                 DONE\n"
        << "========================================\n";

    return 0;
}
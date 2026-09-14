#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"
#include "include/WalkForward.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct CostSensitivityResult
{
    std::string name;

    double fee{};
    double slippage{};

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
// RUN ONE COST SCENARIO
// =========================================================

CostSensitivityResult runCostScenario(
    const std::string& scenarioName,
    double fee,
    double slippage,
    const std::vector<Candle>& candles,
    const std::vector<WalkForwardResult>& baselineResults,
    double initialCapital,
    double stopLossPercentage,
    double takeProfitPercentage)
{
    CostSensitivityResult output;

    output.name = scenarioName;
    output.fee = fee;
    output.slippage = slippage;

    std::vector<EquityPoint> combinedCurve;

    double combinedCapital =
        initialCapital;

    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    std::size_t winningTrades = 0;
    std::size_t losingTrades = 0;

    // Explicit starting point.
    combinedCurve.push_back(
        {
            candles[
                baselineResults.front().testStart
            ].timestamp,
            initialCapital
        }
    );

    // =====================================================
    // REPLAY ALL OOS WINDOWS WITH SAME PARAMETERS
    // =====================================================

    for (const auto& window : baselineResults)
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
                fee,
                slippage,

                window.testStart,
                window.testEnd
            );

        // -------------------------------------------------
        // Aggregate trades
        // -------------------------------------------------

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

        // -------------------------------------------------
        // Stitch this OOS equity curve into the
        // compounded portfolio
        // -------------------------------------------------

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

        // Move portfolio capital to next window.
        combinedCapital =
            combinedCapital *
            (
                testResult.finalCapital /
                initialCapital
            );
    }

    // =====================================================
    // FINAL METRICS
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

    output.totalTrades =
        winningTrades +
        losingTrades;

    output.winningTrades =
        winningTrades;

    output.losingTrades =
        losingTrades;

    if (output.totalTrades > 0)
    {
        output.winRate =
            (
                static_cast<double>(
                    winningTrades
                )
                /
                static_cast<double>(
                    output.totalTrades
                )
            ) * 100.0;
    }

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
            winProbability * averageWin
        )
        +
        (
            lossProbability * averageLoss
        );

    if (totalLosingProfit < 0.0)
    {
        output.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }
    else if (totalWinningProfit > 0.0)
    {
        output.profitFactor = 999999.0;
    }
    else
    {
        output.profitFactor = 0.0;
    }

    calculateDrawdown(
        combinedCurve,
        initialCapital,
        output.maximumDrawdown,
        output.maximumDrawdownPercentage
    );

    output.sharpeRatio =
        calculateSharpe(combinedCurve);

    return output;
}


// =========================================================
// MAIN
// =========================================================

int main()
{
    const std::string dataFile =
        "data/BTCUSDT.csv";

    const double initialCapital = 100000.0;

    const double stopLossPercentage = 0.02;
    const double takeProfitPercentage = 0.04;

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

    const double baselineFee = 0.001;
    const double baselineSlippage = 0.0005;

    // =========================================================
    // WALK-FORWARD CONFIGURATION
    // =========================================================

    const std::size_t trainSize = 4320;
    const std::size_t testSize = 720;

    const std::vector<std::size_t> fastPeriods = {
        3, 5, 8
    };

    const std::vector<std::size_t> slowPeriods = {
        10, 15, 20, 30
    };

    const std::vector<std::size_t> rsiPeriods = {
        7, 14, 21
    };

    const std::vector<double> rsiThresholds = {
        50.0, 55.0, 60.0
    };

    // =========================================================
    // BASELINE WALK-FORWARD
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       BASELINE WALK-FORWARD\n"
        << "========================================\n";

    std::cout
        << "Fee       : "
        << baselineFee * 100.0
        << "%\n";

    std::cout
        << "Slippage  : "
        << baselineSlippage * 100.0
        << "%\n";

    const std::vector<WalkForwardResult> baselineResults =
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

    if (baselineResults.empty())
    {
        std::cerr
            << "\nError: No walk-forward results.\n";

        return 1;
    }

    // =========================================================
    // SHOW SELECTED MODELS
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       SELECTED OOS PARAMETERS\n"
        << "========================================\n";

    for (std::size_t i = 0;
         i < baselineResults.size();
         ++i)
    {
        const auto& result =
            baselineResults[i];

        std::cout
            << "Window "
            << (i + 1)
            << " -> SMA "
            << result.fastPeriod
            << "/"
            << result.slowPeriod
            << ", RSI "
            << result.rsiPeriod
            << ", Threshold "
            << result.rsiBuyThreshold
            << '\n';
    }

    // =========================================================
    // COST SCENARIOS
    // =========================================================

    std::vector<CostSensitivityResult> sensitivityResults;

    sensitivityResults.push_back(
        runCostScenario(
            "Optimistic",
            0.0005,
            0.0002,
            candles,
            baselineResults,
            initialCapital,
            stopLossPercentage,
            takeProfitPercentage
        )
    );

    sensitivityResults.push_back(
        runCostScenario(
            "Current",
            0.0010,
            0.0005,
            candles,
            baselineResults,
            initialCapital,
            stopLossPercentage,
            takeProfitPercentage
        )
    );

    sensitivityResults.push_back(
        runCostScenario(
            "Moderate",
            0.0015,
            0.0010,
            candles,
            baselineResults,
            initialCapital,
            stopLossPercentage,
            takeProfitPercentage
        )
    );

    sensitivityResults.push_back(
        runCostScenario(
            "Harsh",
            0.0020,
            0.0015,
            candles,
            baselineResults,
            initialCapital,
            stopLossPercentage,
            takeProfitPercentage
        )
    );

    sensitivityResults.push_back(
        runCostScenario(
            "Very Harsh",
            0.0025,
            0.0020,
            candles,
            baselineResults,
            initialCapital,
            stopLossPercentage,
            takeProfitPercentage
        )
    );

    // =========================================================
    // PRINT SENSITIVITY TABLE
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       TRANSACTION COST SENSITIVITY\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\nScenario       Fee     Slip"
        << "     Return       Final"
        << "       DD%      Sharpe"
        << "      PF      Expectancy   Trades\n";

    std::cout
        << "-------------------------------------------------------------------------------\n";

    for (const auto& result :
         sensitivityResults)
    {
        std::cout
            << std::left
            << std::setw(15)
            << result.name

            << std::right
            << std::setw(7)
            << result.fee * 100.0

            << std::setw(8)
            << result.slippage * 100.0

            << std::setw(12)
            << result.returnPercentage

            << std::setw(13)
            << result.finalCapital

            << std::setw(11)
            << result.maximumDrawdownPercentage

            << std::setw(11)
            << result.sharpeRatio

            << std::setw(10)
            << result.profitFactor

            << std::setw(14)
            << result.expectancy

            << std::setw(8)
            << result.totalTrades

            << '\n';
    }

    // =========================================================
    // SAVE CSV
    // =========================================================

    std::ofstream outputFile(
        "results/cost_sensitivity.csv"
    );

    if (!outputFile)
    {
        std::cerr
            << "\nError: Could not create "
            << "results/cost_sensitivity.csv\n";
    }
    else
    {
        outputFile
            << "scenario,fee,slippage,final_capital,"
            << "profit_loss,return_percentage,"
            << "maximum_drawdown,"
            << "maximum_drawdown_percentage,"
            << "sharpe_ratio,win_rate,expectancy,"
            << "profit_factor,total_trades,"
            << "winning_trades,losing_trades\n";

        for (const auto& result :
             sensitivityResults)
        {
            outputFile
                << result.name
                << ","
                << result.fee
                << ","
                << result.slippage
                << ","
                << result.finalCapital
                << ","
                << result.profitLoss
                << ","
                << result.returnPercentage
                << ","
                << result.maximumDrawdown
                << ","
                << result.maximumDrawdownPercentage
                << ","
                << result.sharpeRatio
                << ","
                << result.winRate
                << ","
                << result.expectancy
                << ","
                << result.profitFactor
                << ","
                << result.totalTrades
                << ","
                << result.winningTrades
                << ","
                << result.losingTrades
                << '\n';
        }

        outputFile.close();

        std::cout
            << "\nCost sensitivity results saved to:\n"
            << "results/cost_sensitivity.csv\n";
    }

    // =========================================================
    // SIMPLE INTERPRETATION
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          ROBUSTNESS CHECK\n"
        << "========================================\n";

    for (const auto& result :
         sensitivityResults)
    {
        std::cout
            << result.name
            << ": ";

        if (result.returnPercentage > 0.0 &&
            result.expectancy > 0.0 &&
            result.profitFactor > 1.0)
        {
            std::cout
                << "PROFITABLE\n";
        }
        else
        {
            std::cout
                << "NOT PROFITABLE\n";
        }
    }

    std::cout
        << "\n========================================\n"
        << "                 DONE\n"
        << "========================================\n";

    return 0;
}



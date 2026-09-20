#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"
#include "include/WalkForward.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// ================================================================
// OOS METRICS
// ================================================================

struct OOSMetrics
{
    double initialCapital{};
    double finalCapital{};
    double profitLoss{};
    double returnPercentage{};

    double maximumDrawdown{};
    double maximumDrawdownPercentage{};

    double sharpeRatio{};
    double profitFactor{};
    double expectancy{};

    double winRate{};
    double averageWin{};
    double averageLoss{};

    std::size_t totalTrades{};
    std::size_t winningTrades{};
    std::size_t losingTrades{};
};

// ================================================================
// COST SCENARIO
// ================================================================

struct CostScenario
{
    std::string name;
    double feeRate{};
    double slippageRate{};
};

// ================================================================
// CALCULATE MAXIMUM DRAWDOWN
// ================================================================

double calculateDrawdown(
    const std::vector<EquityPoint>& equityCurve,
    double& maximumDrawdownPercentage)
{
    if (equityCurve.empty())
    {
        maximumDrawdownPercentage = 0.0;
        return 0.0;
    }

    double peak = equityCurve.front().equity;

    double maximumDrawdown = 0.0;

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

    return maximumDrawdown;
}

// ================================================================
// CALCULATE RAW SHARPE
// ================================================================

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
        sum /
        static_cast<double>(returns.size());

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
        static_cast<double>(
            returns.size() - 1
        );

    const double standardDeviation =
        std::sqrt(variance);

    if (standardDeviation <= 0.0)
    {
        return 0.0;
    }

    return mean / standardDeviation;
}

// ================================================================
// CALCULATE TRADE METRICS
// ================================================================

void calculateTradeMetrics(
    const std::vector<Trade>& trades,
    OOSMetrics& metrics)
{
    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    metrics.totalTrades = trades.size();

    metrics.winningTrades = 0;
    metrics.losingTrades = 0;

    for (const auto& trade : trades)
    {
        if (trade.profitLoss > 0.0)
        {
            totalWinningProfit +=
                trade.profitLoss;

            ++metrics.winningTrades;
        }
        else if (trade.profitLoss < 0.0)
        {
            totalLosingProfit +=
                trade.profitLoss;

            ++metrics.losingTrades;
        }
    }

    if (metrics.totalTrades > 0)
    {
        metrics.winRate =
            (
                static_cast<double>(
                    metrics.winningTrades
                )
                /
                static_cast<double>(
                    metrics.totalTrades
                )
            ) * 100.0;
    }

    if (metrics.winningTrades > 0)
    {
        metrics.averageWin =
            totalWinningProfit /
            static_cast<double>(
                metrics.winningTrades
            );
    }

    if (metrics.losingTrades > 0)
    {
        metrics.averageLoss =
            totalLosingProfit /
            static_cast<double>(
                metrics.losingTrades
            );
    }

    const double winProbability =
        metrics.totalTrades > 0
            ? static_cast<double>(
                  metrics.winningTrades
              )
              /
              static_cast<double>(
                  metrics.totalTrades
              )
            : 0.0;

    const double lossProbability =
        metrics.totalTrades > 0
            ? static_cast<double>(
                  metrics.losingTrades
              )
              /
              static_cast<double>(
                  metrics.totalTrades
              )
            : 0.0;

    metrics.expectancy =
        (
            winProbability *
            metrics.averageWin
        )
        +
        (
            lossProbability *
            metrics.averageLoss
        );

    if (totalLosingProfit < 0.0)
    {
        metrics.profitFactor =
            totalWinningProfit /
            (-totalLosingProfit);
    }
    else if (totalWinningProfit > 0.0)
    {
        metrics.profitFactor =
            999999.0;
    }
    else
    {
        metrics.profitFactor = 0.0;
    }
}

// ================================================================
// REPLAY OOS WINDOWS
//
// If useVolatilityFilter = true:
//     uses the training-derived ATR threshold stored in each
//     WalkForwardResult.
//
// If false:
//     volatility filter is disabled.
//
// IMPORTANT:
//     Parameters and ATR thresholds are NOT re-optimized.
// ================================================================

OOSMetrics calculateReferenceOOSMetrics(
    const std::vector<Candle>& candles,
    const std::vector<WalkForwardResult>& results,
    double initialCapital,
    double stopLossPercentage,
    double takeProfitPercentage,
    double tradingFeeRate,
    double slippageRate,
    bool useVolatilityFilter,
    std::vector<EquityPoint>* outputEquityCurve = nullptr)
{
    OOSMetrics metrics;

    metrics.initialCapital =
        initialCapital;

    metrics.finalCapital =
        initialCapital;

    std::vector<EquityPoint> combinedEquityCurve;

    combinedEquityCurve.push_back(
        {
            "START",
            initialCapital
        }
    );

    std::vector<Trade> allTrades;

    double compoundedCapital =
        initialCapital;

    for (const auto& window : results)
    {
        BacktestResult testResult;

        if (useVolatilityFilter)
        {
            testResult =
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
        }
        else
        {
            testResult =
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
                    window.testEnd
                );
        }

        // --------------------------------------------------------
        // Aggregate trades
        // --------------------------------------------------------

        for (const auto& trade :
             testResult.trades)
        {
            allTrades.push_back(trade);
        }

        // --------------------------------------------------------
        // Compound the OOS window
        //
        // Each window starts from initialCapital internally.
        // Normalize its equity curve and scale it by the
        // compounded portfolio capital.
        // --------------------------------------------------------

        for (const auto& point :
             testResult.equityCurve)
        {
            const double normalizedEquity =
                point.equity /
                initialCapital;

            const double portfolioEquity =
                compoundedCapital *
                normalizedEquity;

            combinedEquityCurve.push_back(
                {
                    point.timestamp,
                    portfolioEquity
                }
            );
        }

        const double windowReturnFactor =
            testResult.finalCapital /
            initialCapital;

        compoundedCapital *=
            windowReturnFactor;
    }

    metrics.finalCapital =
        compoundedCapital;

    metrics.profitLoss =
        metrics.finalCapital -
        metrics.initialCapital;

    metrics.returnPercentage =
        (
            metrics.finalCapital /
            metrics.initialCapital
            - 1.0
        ) * 100.0;

    // ------------------------------------------------------------
    // Trade metrics
    // ------------------------------------------------------------

    calculateTradeMetrics(
        allTrades,
        metrics
    );

    // ------------------------------------------------------------
    // Drawdown
    // ------------------------------------------------------------

    metrics.maximumDrawdown =
        calculateDrawdown(
            combinedEquityCurve,
            metrics.maximumDrawdownPercentage
        );

    // ------------------------------------------------------------
    // Sharpe
    // ------------------------------------------------------------

    metrics.sharpeRatio =
        calculateSharpe(
            combinedEquityCurve
        );

    if (outputEquityCurve != nullptr)
    {
        *outputEquityCurve =
            combinedEquityCurve;
    }

    return metrics;
}

// ================================================================
// BUY & HOLD BENCHMARK
// ================================================================

OOSMetrics calculateBuyAndHold(
    const std::vector<Candle>& candles,
    std::size_t startIndex,
    std::size_t endIndex,
    double initialCapital)
{
    OOSMetrics metrics;

    metrics.initialCapital =
        initialCapital;

    if (candles.empty() ||
        startIndex >= candles.size())
    {
        return metrics;
    }

    endIndex =
        std::min(
            endIndex,
            candles.size() - 1
        );

    if (startIndex > endIndex)
    {
        return metrics;
    }

    const double startPrice =
        candles[startIndex].open;

    const double endPrice =
        candles[endIndex].close;

    if (startPrice <= 0.0)
    {
        return metrics;
    }

    metrics.finalCapital =
        initialCapital *
        (endPrice / startPrice);

    metrics.profitLoss =
        metrics.finalCapital -
        initialCapital;

    metrics.returnPercentage =
        (
            metrics.finalCapital /
            initialCapital
            - 1.0
        ) * 100.0;

    return metrics;
}

// ================================================================
// SAVE EQUITY CURVE
// ================================================================

void saveEquityCurve(
    const std::string& filename,
    const std::vector<EquityPoint>& equityCurve)
{
    std::ofstream file(filename);

    if (!file)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << '\n';

        return;
    }

    file
        << "timestamp,equity\n";

    file
        << std::fixed
        << std::setprecision(8);

    for (const auto& point :
         equityCurve)
    {
        file
            << point.timestamp
            << ","
            << point.equity
            << '\n';
    }

    file.close();

    std::cout
        << "Saved: "
        << filename
        << '\n';
}

// ================================================================
// SAVE WFV WINDOW RESULTS
// ================================================================

void saveWindowResults(
    const std::string& filename,
    const std::vector<WalkForwardResult>& results)
{
    std::ofstream file(filename);

    if (!file)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << '\n';

        return;
    }

    file
        << "window,"
        << "train_start,"
        << "train_end,"
        << "test_start,"
        << "test_end,"
        << "fast_period,"
        << "slow_period,"
        << "rsi_period,"
        << "rsi_threshold,"
        << "atr_period,"
        << "max_atr_percentage,"
        << "test_profit_loss,"
        << "test_final_capital,"
        << "test_max_drawdown_percentage,"
        << "test_win_rate,"
        << "test_profit_factor,"
        << "test_sharpe,"
        << "test_trades\n";

    file
        << std::fixed
        << std::setprecision(8);

    for (std::size_t i = 0;
         i < results.size();
         ++i)
    {
        const auto& result =
            results[i];

        file
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
            << '\n';
    }

    file.close();

    std::cout
        << "Saved: "
        << filename
        << '\n';
}

// ================================================================
// SAVE OOS COMPARISON
// ================================================================

void saveComparisonCSV(
    const std::string& filename,
    const OOSMetrics& baseline,
    const OOSMetrics& filtered,
    const OOSMetrics& buyAndHold)
{
    std::ofstream file(filename);

    if (!file)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << '\n';

        return;
    }

    file
        << "metric,"
        << "baseline,"
        << "volatility_filtered,"
        << "btc_buy_and_hold\n";

    file
        << std::fixed
        << std::setprecision(8);

    file
        << "final_capital,"
        << baseline.finalCapital
        << ","
        << filtered.finalCapital
        << ","
        << buyAndHold.finalCapital
        << '\n';

    file
        << "profit_loss,"
        << baseline.profitLoss
        << ","
        << filtered.profitLoss
        << ","
        << buyAndHold.profitLoss
        << '\n';

    file
        << "return_percentage,"
        << baseline.returnPercentage
        << ","
        << filtered.returnPercentage
        << ","
        << buyAndHold.returnPercentage
        << '\n';

    file
        << "maximum_drawdown,"
        << baseline.maximumDrawdown
        << ","
        << filtered.maximumDrawdown
        << ","
        << 0.0
        << '\n';

    file
        << "maximum_drawdown_percentage,"
        << baseline.maximumDrawdownPercentage
        << ","
        << filtered.maximumDrawdownPercentage
        << ","
        << 0.0
        << '\n';

    file
        << "sharpe_ratio,"
        << baseline.sharpeRatio
        << ","
        << filtered.sharpeRatio
        << ","
        << 0.0
        << '\n';

    file
        << "profit_factor,"
        << baseline.profitFactor
        << ","
        << filtered.profitFactor
        << ","
        << 0.0
        << '\n';

    file
        << "expectancy,"
        << baseline.expectancy
        << ","
        << filtered.expectancy
        << ","
        << 0.0
        << '\n';

    file
        << "win_rate,"
        << baseline.winRate
        << ","
        << filtered.winRate
        << ","
        << 0.0
        << '\n';

    file
        << "trades,"
        << baseline.totalTrades
        << ","
        << filtered.totalTrades
        << ","
        << 0
        << '\n';

    file
        << "winning_trades,"
        << baseline.winningTrades
        << ","
        << filtered.winningTrades
        << ","
        << 0
        << '\n';

    file
        << "losing_trades,"
        << baseline.losingTrades
        << ","
        << filtered.losingTrades
        << ","
        << 0
        << '\n';

    file.close();

    std::cout
        << "Saved: "
        << filename
        << '\n';
}

// ================================================================
// PRINT OOS SUMMARY
// ================================================================

void printOOSSummary(
    const std::string& title,
    const OOSMetrics& metrics)
{
    std::cout
        << "\n========================================\n"
        << title
        << "\n========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Initial Capital : "
        << metrics.initialCapital
        << '\n';

    std::cout
        << "Final Capital   : "
        << metrics.finalCapital
        << '\n';

    std::cout
        << "Total P&L       : "
        << metrics.profitLoss
        << '\n';

    std::cout
        << "Return          : "
        << metrics.returnPercentage
        << "%\n";

    std::cout
        << "Max Drawdown    : "
        << metrics.maximumDrawdown
        << '\n';

    std::cout
        << "Max DD %        : "
        << metrics.maximumDrawdownPercentage
        << "%\n";

    std::cout
        << "Sharpe          : "
        << metrics.sharpeRatio
        << '\n';

    std::cout
        << "Profit Factor   : "
        << metrics.profitFactor
        << '\n';

    std::cout
        << "Win Rate        : "
        << metrics.winRate
        << "%\n";

    std::cout
        << "Expectancy      : "
        << metrics.expectancy
        << '\n';

    std::cout
        << "Average Win     : "
        << metrics.averageWin
        << '\n';

    std::cout
        << "Average Loss    : "
        << metrics.averageLoss
        << '\n';

    std::cout
        << "Total Trades    : "
        << metrics.totalTrades
        << '\n';

    std::cout
        << "Winning Trades  : "
        << metrics.winningTrades
        << '\n';

    std::cout
        << "Losing Trades   : "
        << metrics.losingTrades
        << '\n';
}

// ================================================================
// PRINT OOS COMPARISON
// ================================================================

void printComparisonTable(
    const OOSMetrics& baseline,
    const OOSMetrics& filtered,
    const OOSMetrics& buyAndHold)
{
    std::cout
        << "\n========================================\n"
        << "             OOS COMPARISON\n"
        << "========================================\n\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << std::left
        << std::setw(30)
        << "Metric"
        << std::right
        << std::setw(16)
        << "Baseline"
        << std::setw(16)
        << "Filtered"
        << std::setw(16)
        << "BTC B&H"
        << '\n';

    std::cout
        << "--------------------------------------------------------------------------\n";

    std::cout
        << std::left
        << std::setw(30)
        << "Final Capital"
        << std::right
        << std::setw(16)
        << baseline.finalCapital
        << std::setw(16)
        << filtered.finalCapital
        << std::setw(16)
        << buyAndHold.finalCapital
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "P&L"
        << std::right
        << std::setw(16)
        << baseline.profitLoss
        << std::setw(16)
        << filtered.profitLoss
        << std::setw(16)
        << buyAndHold.profitLoss
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Return %"
        << std::right
        << std::setw(16)
        << baseline.returnPercentage
        << std::setw(16)
        << filtered.returnPercentage
        << std::setw(16)
        << buyAndHold.returnPercentage
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Max Drawdown %"
        << std::right
        << std::setw(16)
        << baseline.maximumDrawdownPercentage
        << std::setw(16)
        << filtered.maximumDrawdownPercentage
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Sharpe"
        << std::right
        << std::setw(16)
        << baseline.sharpeRatio
        << std::setw(16)
        << filtered.sharpeRatio
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Profit Factor"
        << std::right
        << std::setw(16)
        << baseline.profitFactor
        << std::setw(16)
        << filtered.profitFactor
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Expectancy"
        << std::right
        << std::setw(16)
        << baseline.expectancy
        << std::setw(16)
        << filtered.expectancy
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Win Rate %"
        << std::right
        << std::setw(16)
        << baseline.winRate
        << std::setw(16)
        << filtered.winRate
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Trades"
        << std::right
        << std::setw(16)
        << baseline.totalTrades
        << std::setw(16)
        << filtered.totalTrades
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Winning Trades"
        << std::right
        << std::setw(16)
        << baseline.winningTrades
        << std::setw(16)
        << filtered.winningTrades
        << std::setw(16)
        << "-"
        << '\n';

    std::cout
        << std::left
        << std::setw(30)
        << "Losing Trades"
        << std::right
        << std::setw(16)
        << baseline.losingTrades
        << std::setw(16)
        << filtered.losingTrades
        << std::setw(16)
        << "-"
        << '\n';
}

// ================================================================
// RUN ONE FILTERED COST SCENARIO
//
// IMPORTANT:
//
// Uses the SAME:
// - WFV-selected strategy parameters
// - training-derived ATR thresholds
//
// Only fee and slippage change.
//
// No re-optimization.
// ================================================================

OOSMetrics runCostScenario(
    const CostScenario& scenario,
    const std::vector<Candle>& candles,
    const std::vector<WalkForwardResult>& filteredResults,
    double initialCapital,
    double stopLossPercentage,
    double takeProfitPercentage)
{
    std::vector<EquityPoint> equityCurve;

    return calculateReferenceOOSMetrics(
        candles,
        filteredResults,
        initialCapital,
        stopLossPercentage,
        takeProfitPercentage,
        scenario.feeRate,
        scenario.slippageRate,
        true,
        &equityCurve
    );
}

// ================================================================
// SAVE COST SENSITIVITY
// ================================================================

void saveCostSensitivityCSV(
    const std::string& filename,
    const std::vector<CostScenario>& scenarios,
    const std::vector<OOSMetrics>& metrics)
{
    std::ofstream file(filename);

    if (!file)
    {
        std::cerr
            << "Error: Could not create "
            << filename
            << '\n';

        return;
    }

    file
        << "scenario,"
        << "fee_percent,"
        << "slippage_percent,"
        << "final_capital,"
        << "profit_loss,"
        << "return_percentage,"
        << "maximum_drawdown,"
        << "maximum_drawdown_percentage,"
        << "sharpe_ratio,"
        << "profit_factor,"
        << "expectancy,"
        << "win_rate,"
        << "total_trades,"
        << "winning_trades,"
        << "losing_trades\n";

    file
        << std::fixed
        << std::setprecision(8);

    for (std::size_t i = 0;
         i < scenarios.size();
         ++i)
    {
        const auto& scenario =
            scenarios[i];

        const auto& result =
            metrics[i];

        file
            << scenario.name
            << ","
            << scenario.feeRate * 100.0
            << ","
            << scenario.slippageRate * 100.0
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
            << result.profitFactor
            << ","
            << result.expectancy
            << ","
            << result.winRate
            << ","
            << result.totalTrades
            << ","
            << result.winningTrades
            << ","
            << result.losingTrades
            << '\n';
    }

    file.close();

    std::cout
        << "\nSaved: "
        << filename
        << '\n';
}

// ================================================================
// PRINT COST SENSITIVITY
// ================================================================

void printCostSensitivity(
    const std::vector<CostScenario>& scenarios,
    const std::vector<OOSMetrics>& metrics)
{
    std::cout
        << "\n========================================\n"
        << "       TRANSACTION COST SENSITIVITY\n"
        << "========================================\n\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << std::left
        << std::setw(15)
        << "Scenario"
        << std::right
        << std::setw(9)
        << "Fee %"
        << std::setw(12)
        << "Slip %"
        << std::setw(14)
        << "Final Capital"
        << std::setw(12)
        << "Return %"
        << std::setw(12)
        << "Max DD %"
        << std::setw(10)
        << "PF"
        << std::setw(14)
        << "Expectancy"
        << std::setw(9)
        << "Trades"
        << '\n';

    std::cout
        << "--------------------------------------------------------------------------------\n";

    for (std::size_t i = 0;
         i < scenarios.size();
         ++i)
    {
        const auto& scenario =
            scenarios[i];

        const auto& result =
            metrics[i];

        std::cout
            << std::left
            << std::setw(15)
            << scenario.name

            << std::right
            << std::setw(9)
            << scenario.feeRate * 100.0

            << std::setw(12)
            << scenario.slippageRate * 100.0

            << std::setw(14)
            << result.finalCapital

            << std::setw(12)
            << result.returnPercentage

            << std::setw(12)
            << result.maximumDrawdownPercentage

            << std::setw(10)
            << result.profitFactor

            << std::setw(14)
            << result.expectancy

            << std::setw(9)
            << result.totalTrades

            << '\n';
    }
}

// ================================================================
// MAIN
// ================================================================

int main()
{
    try
    {
        // ============================================================
        // DATA
        // ============================================================

        const std::string dataFile =
            "data/BTCUSDT.csv";

        const double initialCapital =
            100000.0;

        const double stopLossPercentage =
            0.02;

        const double takeProfitPercentage =
            0.04;

        // ============================================================
        // LOAD DATA
        // ============================================================

        const std::vector<Candle> candles =
            readCSV(dataFile);

        if (candles.empty())
        {
            std::cerr
                << "Error: No candles loaded.\n";

            return 1;
        }

        std::cout
            << "\nCSV loaded successfully: "
            << candles.size()
            << " valid candles.\n";

        // ============================================================
        // CURRENT BASELINE COSTS
        // ============================================================

        const double baselineFee =
            0.001;

        const double baselineSlippage =
            0.0005;

        // ============================================================
        // WALK-FORWARD CONFIGURATION
        // ============================================================

        const std::size_t trainSize =
            4320;

        const std::size_t testSize =
            720;

        const std::vector<std::size_t>
            fastPeriods =
        {
            3,
            5,
            8
        };

        const std::vector<std::size_t>
            slowPeriods =
        {
            10,
            15,
            20,
            30
        };

        const std::vector<std::size_t>
            rsiPeriods =
        {
            7,
            14,
            21
        };

        const std::vector<double>
            rsiThresholds =
        {
            50.0,
            55.0,
            60.0
        };

        // ============================================================
        // WALK-FORWARD VALIDATION
        //
        // The WFV itself derives volatility thresholds from
        // training data only.
        // ============================================================

        std::cout
            << "\n========================================\n"
            << "       VOLATILITY-FILTERED WFV\n"
            << "========================================\n";

        std::cout
            << "Training Size : "
            << trainSize
            << '\n';

        std::cout
            << "Test Size     : "
            << testSize
            << '\n';

        std::cout
            << "Fee           : "
            << baselineFee * 100.0
            << "%\n";

        std::cout
            << "Slippage      : "
            << baselineSlippage * 100.0
            << "%\n";

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
                << "\nError: No walk-forward "
                   "results generated.\n";

            return 1;
        }

        // ============================================================
        // SELECTED PARAMETERS
        // ============================================================

        std::cout
            << "\n========================================\n"
            << "       SELECTED OOS PARAMETERS\n"
            << "========================================\n";

        for (std::size_t i = 0;
             i < filteredResults.size();
             ++i)
        {
            const auto& result =
                filteredResults[i];

            std::cout
                << "\nWindow "
                << (i + 1)
                << '\n';

            std::cout
                << "  Train: "
                << result.trainStart
                << " -> "
                << result.trainEnd
                << '\n';

            std::cout
                << "  Test : "
                << result.testStart
                << " -> "
                << result.testEnd
                << '\n';

            std::cout
                << "  SMA  : "
                << result.fastPeriod
                << "/"
                << result.slowPeriod
                << '\n';

            std::cout
                << "  RSI  : "
                << result.rsiPeriod
                << '\n';

            std::cout
                << "  RSI Threshold: "
                << result.rsiBuyThreshold
                << '\n';

            std::cout
                << "  ATR Period: "
                << result.atrPeriod
                << '\n';

            std::cout
                << "  Max ATR %: "
                << result.maxAtrPercentage
                << '\n';
        }

        // ============================================================
        // FILTERED OOS METRICS
        // ============================================================

        std::vector<EquityPoint>
            filteredEquityCurve;

        const OOSMetrics filteredMetrics =
            calculateReferenceOOSMetrics(
                candles,

                filteredResults,

                initialCapital,

                stopLossPercentage,
                takeProfitPercentage,

                baselineFee,
                baselineSlippage,

                true,

                &filteredEquityCurve
            );

        printOOSSummary(
            "FILTERED OOS SUMMARY",
            filteredMetrics
        );

        // ============================================================
        // BASELINE REFERENCE
        //
        // Same selected WFV parameters.
        // Volatility filter disabled.
        //
        // This isolates the effect of the volatility filter.
        // ============================================================

        std::vector<EquityPoint>
            baselineEquityCurve;

        const OOSMetrics baselineMetrics =
            calculateReferenceOOSMetrics(
                candles,

                filteredResults,

                initialCapital,

                stopLossPercentage,
                takeProfitPercentage,

                baselineFee,
                baselineSlippage,

                false,

                &baselineEquityCurve
            );

        printOOSSummary(
            "BASELINE REFERENCE OOS SUMMARY",
            baselineMetrics
        );

        // ============================================================
        // BTC BUY & HOLD
        // ============================================================

        const std::size_t firstTestStart =
            filteredResults.front().testStart;

        const std::size_t lastTestEnd =
            filteredResults.back().testEnd;

        const OOSMetrics buyAndHold =
            calculateBuyAndHold(
                candles,
                firstTestStart,
                lastTestEnd,
                initialCapital
            );

        printOOSSummary(
            "BTC BUY & HOLD OOS BENCHMARK",
            buyAndHold
        );

        // ============================================================
        // COMPARISON TABLE
        // ============================================================

        printComparisonTable(
            baselineMetrics,
            filteredMetrics,
            buyAndHold
        );

        // ============================================================
        // SAVE EQUITY CURVES
        // ============================================================

        saveEquityCurve(
            "results/volatility_filtered_oos_equity.csv",
            filteredEquityCurve
        );

        saveEquityCurve(
            "results/baseline_oos_equity.csv",
            baselineEquityCurve
        );

        // ============================================================
        // SAVE WFV RESULTS
        // ============================================================

        saveWindowResults(
            "results/volatility_filtered_wfv.csv",
            filteredResults
        );

        // ============================================================
        // SAVE STRATEGY COMPARISON
        // ============================================================

        saveComparisonCSV(
            "results/oos_strategy_comparison.csv",

            baselineMetrics,
            filteredMetrics,
            buyAndHold
        );

        // ============================================================
        // TRANSACTION COST SENSITIVITY
        //
        // VERY IMPORTANT:
        //
        // We do NOT re-run optimization.
        //
        // The following remain fixed:
        //
        // 1. WFV-selected SMA parameters
        // 2. WFV-selected RSI parameters
        // 3. Training-derived ATR thresholds
        //
        // Only transaction costs change.
        // ============================================================

        const std::vector<CostScenario>
            costScenarios =
        {
            {
                "Optimistic",
                0.0005,
                0.0002
            },

            {
                "Current",
                0.0010,
                0.0005
            },

            {
                "Moderate",
                0.0015,
                0.0010
            },

            {
                "Harsh",
                0.0020,
                0.0015
            },

            {
                "Very Harsh",
                0.0025,
                0.0020
            }
        };

        std::vector<OOSMetrics>
            costSensitivityMetrics;

        for (const auto& scenario :
             costScenarios)
        {
            const OOSMetrics result =
                runCostScenario(
                    scenario,

                    candles,

                    filteredResults,

                    initialCapital,

                    stopLossPercentage,
                    takeProfitPercentage
                );

            costSensitivityMetrics.push_back(
                result
            );
        }

        // ============================================================
        // PRINT COST SENSITIVITY
        // ============================================================

        printCostSensitivity(
            costScenarios,
            costSensitivityMetrics
        );

        // ============================================================
        // SAVE COST SENSITIVITY
        // ============================================================

        saveCostSensitivityCSV(
            "results/volatility_filtered_cost_sensitivity.csv",

            costScenarios,
            costSensitivityMetrics
        );

        // ============================================================
        // FINAL STATUS
        // ============================================================

        std::cout
            << "\n========================================\n"
            << "          RESEARCH STATUS\n"
            << "========================================\n";

        std::cout
            << "\nVolatility Filter:\n";

        std::cout
            << "  Baseline Return : "
            << baselineMetrics.returnPercentage
            << "%\n";

        std::cout
            << "  Filtered Return : "
            << filteredMetrics.returnPercentage
            << "%\n";

        std::cout
            << "  Baseline DD     : "
            << baselineMetrics.maximumDrawdownPercentage
            << "%\n";

        std::cout
            << "  Filtered DD     : "
            << filteredMetrics.maximumDrawdownPercentage
            << "%\n";

        std::cout
            << "  Baseline PF     : "
            << baselineMetrics.profitFactor
            << '\n';

        std::cout
            << "  Filtered PF     : "
            << filteredMetrics.profitFactor
            << '\n';

        std::cout
            << "\nCost Sensitivity:\n";

        for (std::size_t i = 0;
             i < costScenarios.size();
             ++i)
        {
            const auto& scenario =
                costScenarios[i];

            const auto& result =
                costSensitivityMetrics[i];

            std::cout
                << "  "
                << scenario.name
                << " -> Return "
                << result.returnPercentage
                << "%, PF "
                << result.profitFactor
                << ", Expectancy "
                << result.expectancy
                << '\n';
        }

        std::cout
            << "\n========================================\n"
            << "                 DONE\n"
            << "========================================\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "\nError: "
            << e.what()
            << '\n';

        return 1;
    }

    return 0;
}
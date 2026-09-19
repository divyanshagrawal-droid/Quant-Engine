#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"
#include "include/WalkForward.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


// =========================================================
// OOS METRICS
// =========================================================

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
// COMPARISON RESULT
// =========================================================

struct ComparisonResult
{
    std::string name;

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
// CALCULATE FILTERED OOS METRICS
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

    output.equityCurve.push_back(
        {
            candles[results.front().testStart].timestamp,
            initialCapital
        }
    );

    // =====================================================
    // REPLAY FILTERED OOS WINDOWS
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

        // -------------------------------------------------
        // TRADE STATISTICS
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
        // STITCH EQUITY CURVE
        // -------------------------------------------------

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

        // -------------------------------------------------
        // COMPOUND CAPITAL
        // -------------------------------------------------

        combinedCapital *=
            testResult.finalCapital /
            initialCapital;
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

    // =====================================================
    // AVERAGE WIN / LOSS
    // =====================================================

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
// CALCULATE BASELINE / FILTERED REFERENCE METRICS
// =========================================================

ComparisonResult calculateReferenceOOSMetrics(
    const std::vector<Candle>& candles,
    const std::vector<WalkForwardResult>& results,
    double initialCapital,
    double tradingFeeRate,
    double slippageRate,
    double stopLossPercentage,
    double takeProfitPercentage,
    bool volatilityFilterEnabled)
{
    ComparisonResult output;

    double combinedCapital =
        initialCapital;

    double totalWinningProfit = 0.0;
    double totalLosingProfit = 0.0;

    std::size_t winningTrades = 0;
    std::size_t losingTrades = 0;

    std::vector<EquityPoint> combinedCurve;

    if (results.empty())
    {
        return output;
    }

    combinedCurve.push_back(
        {
            candles[results.front().testStart].timestamp,
            initialCapital
        }
    );

    // =====================================================
    // REPLAY EACH OOS WINDOW
    // =====================================================

    for (const auto& window : results)
    {
        double atrThreshold = 0.0;

        if (volatilityFilterEnabled)
        {
            atrThreshold =
                window.maxAtrPercentage;
        }

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
                atrThreshold
            );

        // =================================================
        // TRADE STATISTICS
        // =================================================

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

        // =================================================
        // STITCH EQUITY CURVE
        // =================================================

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

        // =================================================
        // COMPOUND CAPITAL
        // =================================================

        combinedCapital *=
            testResult.finalCapital /
            initialCapital;
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

    // =====================================================
    // EXPECTANCY
    // =====================================================

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
        winProbability * averageWin +
        lossProbability * averageLoss;

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
        combinedCurve,
        initialCapital,
        output.maximumDrawdown,
        output.maximumDrawdownPercentage
    );

    // =====================================================
    // SHARPE
    // =====================================================

    output.sharpeRatio =
        calculateSharpe(combinedCurve);

    return output;
}


// =========================================================
// BTC BUY & HOLD
// =========================================================

ComparisonResult calculateBuyAndHold(
    const std::vector<Candle>& candles,
    std::size_t startIndex,
    std::size_t endIndex,
    double initialCapital)
{
    ComparisonResult output;

    if (startIndex >= candles.size() ||
        endIndex >= candles.size() ||
        startIndex >= endIndex)
    {
        return output;
    }

    const double startPrice =
        candles[startIndex].open;

    const double endPrice =
        candles[endIndex].close;

    if (startPrice <= 0.0)
    {
        return output;
    }

    output.name =
        "BTC Buy & Hold";

    output.finalCapital =
        initialCapital *
        (endPrice / startPrice);

    output.profitLoss =
        output.finalCapital -
        initialCapital;

    output.returnPercentage =
        (
            (endPrice / startPrice)
            - 1.0
        ) * 100.0;

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
        << "test_dd_percentage,"
        << "test_win_rate,"
        << "test_profit_factor,"
        << "test_sharpe,"
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
// SAVE COMPARISON CSV
// =========================================================

void saveComparisonCSV(
    const std::string& filename,
    const ComparisonResult& baseline,
    const ComparisonResult& filtered,
    const ComparisonResult& btcBuyHold)
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
        << std::fixed
        << std::setprecision(8);

    outputFile
        << "metric,"
        << "baseline,"
        << "volatility_filtered,"
        << "btc_buy_hold\n";

    outputFile
        << "final_capital,"
        << baseline.finalCapital
        << ","
        << filtered.finalCapital
        << ","
        << btcBuyHold.finalCapital
        << "\n";

    outputFile
        << "profit_loss,"
        << baseline.profitLoss
        << ","
        << filtered.profitLoss
        << ","
        << btcBuyHold.profitLoss
        << "\n";

    outputFile
        << "return_percentage,"
        << baseline.returnPercentage
        << ","
        << filtered.returnPercentage
        << ","
        << btcBuyHold.returnPercentage
        << "\n";

    outputFile
        << "maximum_drawdown_percentage,"
        << baseline.maximumDrawdownPercentage
        << ","
        << filtered.maximumDrawdownPercentage
        << ",\n";

    outputFile
        << "sharpe_ratio,"
        << baseline.sharpeRatio
        << ","
        << filtered.sharpeRatio
        << ",\n";

    outputFile
        << "profit_factor,"
        << baseline.profitFactor
        << ","
        << filtered.profitFactor
        << ",\n";

    outputFile
        << "expectancy,"
        << baseline.expectancy
        << ","
        << filtered.expectancy
        << ",\n";

    outputFile
        << "win_rate,"
        << baseline.winRate
        << ","
        << filtered.winRate
        << ",\n";

    outputFile
        << "total_trades,"
        << baseline.totalTrades
        << ","
        << filtered.totalTrades
        << ",\n";

    outputFile
        << "winning_trades,"
        << baseline.winningTrades
        << ","
        << filtered.winningTrades
        << ",\n";

    outputFile
        << "losing_trades,"
        << baseline.losingTrades
        << ","
        << filtered.losingTrades
        << ",\n";

    outputFile.close();

    std::cout
        << "\nComparison saved to:\n"
        << filename
        << "\n";
}


// =========================================================
// PRINT FILTERED OOS SUMMARY
// =========================================================

void printOOSSummary(
    const OOSMetrics& metrics,
    double initialCapital)
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
        << initialCapital
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
// PRINT COMPARISON TABLE
// =========================================================

void printComparisonTable(
    const ComparisonResult& baseline,
    const ComparisonResult& filtered,
    const ComparisonResult& btcBuyHold)
{
    std::cout
        << "\n========================================\n"
        << "       OOS STRATEGY COMPARISON\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\n"
        << std::left
        << std::setw(24)
        << "Metric"

        << std::right
        << std::setw(14)
        << "Baseline"

        << std::setw(16)
        << "Filtered"

        << std::setw(14)
        << "BTC B&H"
        << "\n";

    std::cout
        << "--------------------------------------------------------------------\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Final Capital"

        << std::right
        << std::setw(14)
        << baseline.finalCapital

        << std::setw(16)
        << filtered.finalCapital

        << std::setw(14)
        << btcBuyHold.finalCapital
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "P&L"

        << std::right
        << std::setw(14)
        << baseline.profitLoss

        << std::setw(16)
        << filtered.profitLoss

        << std::setw(14)
        << btcBuyHold.profitLoss
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Return %"

        << std::right
        << std::setw(14)
        << baseline.returnPercentage

        << std::setw(16)
        << filtered.returnPercentage

        << std::setw(14)
        << btcBuyHold.returnPercentage
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Max Drawdown %"

        << std::right
        << std::setw(14)
        << baseline.maximumDrawdownPercentage

        << std::setw(16)
        << filtered.maximumDrawdownPercentage

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Sharpe"

        << std::right
        << std::setw(14)
        << baseline.sharpeRatio

        << std::setw(16)
        << filtered.sharpeRatio

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Profit Factor"

        << std::right
        << std::setw(14)
        << baseline.profitFactor

        << std::setw(16)
        << filtered.profitFactor

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Expectancy"

        << std::right
        << std::setw(14)
        << baseline.expectancy

        << std::setw(16)
        << filtered.expectancy

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Win Rate %"

        << std::right
        << std::setw(14)
        << baseline.winRate

        << std::setw(16)
        << filtered.winRate

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Trades"

        << std::right
        << std::setw(14)
        << baseline.totalTrades

        << std::setw(16)
        << filtered.totalTrades

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Winning Trades"

        << std::right
        << std::setw(14)
        << baseline.winningTrades

        << std::setw(16)
        << filtered.winningTrades

        << std::setw(14)
        << "-"
        << "\n";

    std::cout
        << std::left
        << std::setw(24)
        << "Losing Trades"

        << std::right
        << std::setw(14)
        << baseline.losingTrades

        << std::setw(16)
        << filtered.losingTrades

        << std::setw(14)
        << "-"
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
    // BASELINE TRANSACTION COSTS
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
    // FILTERED OOS METRICS
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
        filteredMetrics,
        initialCapital
    );

    // =========================================================
    // BASELINE REFERENCE
    // =========================================================

    const ComparisonResult baselineComparison =
        calculateReferenceOOSMetrics(
            candles,
            filteredResults,
            initialCapital,
            baselineFee,
            baselineSlippage,
            stopLossPercentage,
            takeProfitPercentage,
            false
        );

    // =========================================================
    // FILTERED REFERENCE
    // =========================================================

    const ComparisonResult filteredComparison =
        calculateReferenceOOSMetrics(
            candles,
            filteredResults,
            initialCapital,
            baselineFee,
            baselineSlippage,
            stopLossPercentage,
            takeProfitPercentage,
            true
        );

    // =========================================================
    // BTC BUY & HOLD
    // =========================================================

    const ComparisonResult btcBuyHold =
        calculateBuyAndHold(
            candles,
            filteredResults.front().testStart,
            filteredResults.back().testEnd,
            initialCapital
        );

    // =========================================================
    // SAVE FILTERED EQUITY CURVE
    // =========================================================

    saveEquityCurve(
        "results/volatility_filtered_oos_equity.csv",
        filteredMetrics.equityCurve
    );

    // =========================================================
    // SAVE WFV WINDOW RESULTS
    // =========================================================

    saveWindowResults(
        "results/volatility_filtered_wfv.csv",
        filteredResults
    );

    // =========================================================
    // FILTERED OOS WINDOW SUMMARY
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "       FILTERED OOS WINDOW SUMMARY\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\n"
        << std::left
        << std::setw(8)
        << "Window"

        << std::setw(14)
        << "P&L"

        << std::setw(12)
        << "Return"

        << std::setw(10)
        << "DD%"

        << std::setw(10)
        << "PF"

        << std::setw(10)
        << "Trades"
        << "\n";

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
            << std::left
            << std::setw(8)
            << (i + 1)

            << std::setw(14)
            << result.testProfitLoss

            << std::setw(12)
            << windowReturn

            << std::setw(10)
            << result.testMaximumDrawdownPercentage

            << std::setw(10)
            << result.testProfitFactor

            << std::setw(10)
            << result.testTrades

            << "\n";
    }

    // =========================================================
    // OOS STRATEGY COMPARISON
    // =========================================================

    printComparisonTable(
        baselineComparison,
        filteredComparison,
        btcBuyHold
    );

    // =========================================================
    // SAVE COMPARISON CSV
    // =========================================================

    saveComparisonCSV(
        "results/oos_strategy_comparison.csv",
        baselineComparison,
        filteredComparison,
        btcBuyHold
    );

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
    // FILTERED OOS INTERPRETATION
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
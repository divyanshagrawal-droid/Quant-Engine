#include "include/Backtester.hpp"
#include "include/Optimizer.hpp"
#include "include/CSVReader.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void printResult(
    const std::string& title,
    const BacktestResult& result)
{
    std::cout
        << "\n========================================\n"
        << title << '\n'
        << "========================================\n\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout << "Initial Capital : "
              << result.initialCapital << '\n';

    std::cout << "Final Capital   : "
              << result.finalCapital << '\n';

    std::cout << "Total P&L       : "
              << result.totalProfitLoss << '\n';

    std::cout << "Trades          : "
              << result.trades.size() << '\n';

    std::cout << "Win Rate        : "
              << result.winRate << "%\n";

    std::cout << "Profit Factor   : "
              << result.profitFactor << '\n';

    std::cout << "Max Drawdown    : "
              << result.maximumDrawdownPercentage
              << "%\n";

    std::cout << "Sharpe Ratio    : "
              << result.sharpeRatio << '\n';
}
}

int main()
{
    try
    {
        const auto candles =
            readCSV("data/BTCUSDT.csv");

        if (candles.empty())
        {
            std::cerr << "No candles loaded.\n";
            return 1;
        }

        constexpr double initialCapital = 100000.0;
        constexpr double tradingFeeRate = 0.001;
        constexpr double slippageRate = 0.0005;

        constexpr std::size_t fastPeriod = 2;
        constexpr std::size_t slowPeriod = 3;

        std::cout
            << "Candles loaded: "
            << candles.size()
            << "\n";

        // =========================================================
        // BASELINE BACKTEST
        // Execution is now next-candle OPEN.
        // =========================================================
        const BacktestResult baseline =
            runBacktest(
                candles,
                fastPeriod,
                slowPeriod,
                initialCapital,
                tradingFeeRate,
                slippageRate
            );

        printResult(
            "BASELINE SMA 2/3",
            baseline
        );

        // =========================================================
        // 70 / 30 TRAIN-TEST SPLIT
        // =========================================================
        if (candles.size() < 100)
        {
            std::cerr
                << "\nNot enough candles for a meaningful "
                   "train/test split.\n";
            return 1;
        }

        const std::size_t splitIndex =
            static_cast<std::size_t>(
                candles.size() * 0.70
            );

        if (splitIndex == 0 ||
            splitIndex >= candles.size() - 1)
        {
            std::cerr
                << "\nInvalid train/test split.\n";
            return 1;
        }

        const std::size_t trainEnd =
            splitIndex - 1;

        const std::size_t testStart =
            splitIndex;

        const std::size_t testEnd =
            candles.size() - 1;

        std::cout
            << "\n========================================\n"
            << "          TRAIN / TEST SPLIT\n"
            << "========================================\n\n";

        std::cout
            << "Train candles : 0 - "
            << trainEnd
            << " ("
            << (trainEnd + 1)
            << " candles)\n";

        std::cout
            << "Test candles  : "
            << testStart
            << " - "
            << testEnd
            << " ("
            << (testEnd - testStart + 1)
            << " candles)\n";

        std::cout
            << "Split ratio    : 70% / 30%\n";

        // =========================================================
        // TRAIN-ONLY OPTIMIZATION
        // =========================================================
        const std::vector<std::size_t> fastPeriods =
        {
            2, 3, 5, 10, 20
        };

        const std::vector<std::size_t> slowPeriods =
        {
            3, 5, 10, 20, 30, 50
        };

        // The existing optimizer runs on a supplied vector, so
        // create a training-only dataset for parameter selection.
        const std::vector<Candle> trainCandles(
            candles.begin(),
            candles.begin() + splitIndex
        );

        const auto optimizationResults =
            optimizeSMA(
                trainCandles,
                fastPeriods,
                slowPeriods,
                initialCapital,
                tradingFeeRate,
                slippageRate
            );

        std::cout
            << "\n========================================\n"
            << "       TRAINING OPTIMIZATION\n"
            << "========================================\n\n";

        std::cout
            << std::left
            << std::setw(8)  << "Fast"
            << std::setw(8)  << "Slow"
            << std::setw(14) << "P&L"
            << std::setw(12) << "PF"
            << std::setw(12) << "DD%"
            << std::setw(12) << "Win Rate"
            << std::setw(12) << "Sharpe"
            << std::setw(10) << "Trades"
            << '\n';

        std::cout
            << std::string(88, '-')
            << '\n';

        for (const auto& r : optimizationResults)
        {
            std::cout
                << std::left
                << std::setw(8) << r.fastPeriod
                << std::setw(8) << r.slowPeriod
                << std::setw(14) << r.totalProfitLoss
                << std::setw(12) << r.profitFactor
                << std::setw(12) << r.maximumDrawdownPercentage
                << std::setw(12) << r.winRate
                << std::setw(12) << r.sharpeRatio
                << std::setw(10) << r.totalTrades
                << '\n';
        }

        if (optimizationResults.empty())
        {
            std::cerr
                << "\nNo valid optimization result found.\n";
            return 1;
        }

        const auto& best =
            optimizationResults.front();

        std::cout
            << "\n========================================\n"
            << "       SELECTED TRAINING MODEL\n"
            << "========================================\n\n";

        std::cout
            << "Fast SMA       : "
            << best.fastPeriod << '\n';

        std::cout
            << "Slow SMA       : "
            << best.slowPeriod << '\n';

        std::cout
            << "Train P&L      : "
            << best.totalProfitLoss << '\n';

        std::cout
            << "Train PF       : "
            << best.profitFactor << '\n';

        // =========================================================
        // OUT-OF-SAMPLE TEST
        //
        // Important: the backtester uses the ORIGINAL candle array
        // for SMA history. Therefore the first test signal can use
        // the completed train candle immediately before the split.
        // No test candle is used to choose parameters.
        // =========================================================
        const BacktestResult testResult =
            runBacktest(
                candles,
                best.fastPeriod,
                best.slowPeriod,
                initialCapital,
                tradingFeeRate,
                slippageRate,
                testStart,
                testEnd
            );

        printResult(
            "OUT-OF-SAMPLE TEST",
            testResult
        );

        std::cout
            << "\n========================================\n"
            << "             FINAL VERDICT\n"
            << "========================================\n\n";

        std::cout
            << "Selected SMA   : "
            << best.fastPeriod
            << "/"
            << best.slowPeriod
            << '\n';

        std::cout
            << "Train P&L      : "
            << best.totalProfitLoss
            << '\n';

        std::cout
            << "Test P&L       : "
            << testResult.totalProfitLoss
            << '\n';

        std::cout
            << "Test PF        : "
            << testResult.profitFactor
            << '\n';

        std::cout
            << "Test Win Rate  : "
            << testResult.winRate
            << "%\n";

        if (testResult.totalProfitLoss > 0.0 &&
            testResult.profitFactor > 1.0)
        {
            std::cout
                << "\nSTATUS: PROFITABLE OUT-OF-SAMPLE\n";
        }
        else
        {
            std::cout
                << "\nSTATUS: NOT PROFITABLE OUT-OF-SAMPLE\n";
            std::cout
                << "Do not treat this model as production-ready.\n";
        }

        std::cout
            << "\n========================================\n"
            << "              DONE\n"
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

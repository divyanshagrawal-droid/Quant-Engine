#include "include/Backtester.hpp"
#include "include/Optimizer.hpp"
#include "include/CSVReader.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    try
    {
        // ============================================================
        // LOAD HISTORICAL MARKET DATA
        // ============================================================

        const auto candles =
            readCSV("data/BTCUSDT.csv");

        std::cout
            << "Candles loaded: "
            << candles.size()
            << "\n\n";


        // ============================================================
        // STRATEGY PARAMETERS
        // ============================================================

        constexpr std::size_t fastPeriod = 2;
        constexpr std::size_t slowPeriod = 3;


        // ============================================================
        // BACKTEST PARAMETERS
        // ============================================================

        constexpr double initialCapital = 100000.0;

        // 0.10% trading fee
        constexpr double tradingFeeRate = 0.001;

        // 0.05% slippage
        constexpr double slippageRate = 0.0005;


        // ============================================================
        // RUN NORMAL BACKTEST
        // ============================================================

        const BacktestResult result =
            runBacktest(
                candles,
                fastPeriod,
                slowPeriod,
                initialCapital,
                tradingFeeRate,
                slippageRate
            );


        // ============================================================
        // BACKTEST SUMMARY
        // ============================================================

        std::cout
            << "========================================\n"
            << "           BACKTEST RESULTS\n"
            << "========================================\n\n";

        std::cout
            << std::fixed
            << std::setprecision(2);

        std::cout
            << "Initial Capital : "
            << result.initialCapital
            << '\n';

        std::cout
            << "Final Capital   : "
            << result.finalCapital
            << '\n';

        std::cout
            << "Total P&L       : "
            << result.totalProfitLoss
            << '\n';

        std::cout
            << "Trades          : "
            << result.trades.size()
            << "\n\n";


        // ============================================================
        // TRADE DETAILS
        // ============================================================

        for (std::size_t i = 0;
             i < result.trades.size();
             ++i)
        {
            const Trade& trade = result.trades[i];

            std::cout
                << "Trade "
                << (i + 1)
                << ":\n";

            std::cout
                << " Entry "
                << trade.entryTime
                << " @ "
                << trade.entryPrice
                << '\n';

            std::cout
                << " Exit  "
                << trade.exitTime
                << " @ "
                << trade.exitPrice
                << '\n';

            std::cout
                << " Quantity   : "
                << trade.quantity
                << '\n';

            std::cout
                << " Entry Fee  : "
                << trade.entryFee
                << '\n';

            std::cout
                << " Exit Fee   : "
                << trade.exitFee
                << '\n';

            std::cout
                << " Gross P&L  : "
                << trade.grossProfitLoss
                << '\n';

            std::cout
                << " Total Fees : "
                << trade.totalFees
                << '\n';

            std::cout
                << " Net P&L    : "
                << trade.profitLoss
                << "\n\n";
        }


        // ============================================================
        // PERFORMANCE METRICS
        // ============================================================

        std::cout
            << "========================================\n"
            << "          PERFORMANCE METRICS\n"
            << "========================================\n\n";

        std::cout
            << "Maximum Drawdown     : "
            << result.maximumDrawdown
            << '\n';

        std::cout
            << "Maximum Drawdown %   : "
            << result.maximumDrawdownPercentage
            << "%\n";

        std::cout
            << "Winning Trades       : "
            << result.winningTrades
            << '\n';

        std::cout
            << "Losing Trades        : "
            << result.losingTrades
            << '\n';

        std::cout
            << "Win Rate             : "
            << result.winRate
            << "%\n";

        std::cout
            << "Average Win          : "
            << result.averageWin
            << '\n';

        std::cout
            << "Average Loss         : "
            << result.averageLoss
            << '\n';

        std::cout
            << "Profit Factor        : "
            << result.profitFactor
            << '\n';

        std::cout
            << "Sharpe Ratio         : "
            << result.sharpeRatio
            << "\n\n";


        // ============================================================
        // EQUITY CURVE
        // ============================================================

        std::cout
            << "========================================\n"
            << "             EQUITY CURVE\n"
            << "========================================\n\n";

        for (const auto& point : result.equityCurve)
        {
            std::cout
                << point.timestamp
                << " | Equity: "
                << point.equity
                << '\n';
        }


        // ============================================================
        // SMA STRATEGY OPTIMIZATION
        // ============================================================

        const std::vector<std::size_t> fastPeriods =
        {
            2,
            3,
            5,
            10,
            20
        };

        const std::vector<std::size_t> slowPeriods =
        {
            3,
            5,
            10,
            20,
            30,
            50
        };


        const std::vector<OptimizationResult> optimizationResults =
            optimizeSMA(
                candles,
                fastPeriods,
                slowPeriods,
                initialCapital,
                tradingFeeRate,
                slippageRate
            );


        // ============================================================
        // OPTIMIZATION RESULTS
        // ============================================================

        std::cout
            << "\n========================================\n"
            << "       SMA STRATEGY OPTIMIZATION\n"
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

        std::cout
            << std::fixed
            << std::setprecision(2);

        for (const auto& optimizationResult :
             optimizationResults)
        {
            std::cout
                << std::left
                << std::setw(8)
                << optimizationResult.fastPeriod

                << std::setw(8)
                << optimizationResult.slowPeriod

                << std::setw(14)
                << optimizationResult.totalProfitLoss

                << std::setw(12)
                << optimizationResult.profitFactor

                << std::setw(12)
                << optimizationResult.maximumDrawdownPercentage

                << std::setw(12)
                << optimizationResult.winRate

                << std::setw(12)
                << optimizationResult.sharpeRatio

                << std::setw(10)
                << optimizationResult.totalTrades

                << '\n';
        }


        // ============================================================
        // BEST STRATEGY
        // ============================================================

        if (!optimizationResults.empty())
        {
            const OptimizationResult& best =
                optimizationResults.front();

            std::cout
                << "\n========================================\n"
                << "             BEST STRATEGY\n"
                << "========================================\n\n";

            std::cout
                << "Fast SMA       : "
                << best.fastPeriod
                << '\n';

            std::cout
                << "Slow SMA       : "
                << best.slowPeriod
                << '\n';

            std::cout
                << "Final Capital  : "
                << best.finalCapital
                << '\n';

            std::cout
                << "Total P&L      : "
                << best.totalProfitLoss
                << '\n';

            std::cout
                << "Profit Factor  : "
                << best.profitFactor
                << '\n';

            std::cout
                << "Win Rate       : "
                << best.winRate
                << "%\n";

            std::cout
                << "Max Drawdown   : "
                << best.maximumDrawdownPercentage
                << "%\n";

            std::cout
                << "Sharpe Ratio   : "
                << best.sharpeRatio
                << '\n';

            std::cout
                << "Total Trades   : "
                << best.totalTrades
                << '\n';
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
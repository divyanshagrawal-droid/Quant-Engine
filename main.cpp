#include "include/Backtester.hpp"
#include "include/Candle.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

std::vector<Candle> readCSV(const std::string& filename);

int main() {
    try {
        // ================================================
        // Load historical market data
        // ================================================
        const auto candles =
            readCSV("data/BTCUSDT.csv");

        std::cout << "Candles loaded: "
                  << candles.size()
                  << "\n\n";

        // ================================================
        // Strategy parameters
        // ================================================
        constexpr std::size_t fastPeriod = 2;
        constexpr std::size_t slowPeriod = 3;

        // ================================================
        // Backtest parameters
        // ================================================
        constexpr double initialCapital = 100000.0;

        // 0.10% trading fee
        constexpr double tradingFeeRate = 0.001;

        // 0.05% slippage
        constexpr double slippageRate = 0.0005;

        // ================================================
        // Run backtest
        // ================================================
        const BacktestResult result =
            runBacktest(
                candles,
                fastPeriod,
                slowPeriod,
                initialCapital,
                tradingFeeRate,
                slippageRate
            );

        // Display monetary values with 2 decimal places.
        std::cout << std::fixed
                  << std::setprecision(2);

        // ================================================
        // Overall backtest result
        // ================================================
        std::cout << "===== BACKTEST RESULT =====\n\n";

        std::cout << "Initial Capital : "
                  << result.initialCapital
                  << '\n';

        std::cout << "Final Capital   : "
                  << result.finalCapital
                  << '\n';

        std::cout << "Total P&L       : "
                  << result.totalProfitLoss
                  << '\n';

        // ================================================
        // Equity curve
        // ================================================
        std::cout << "\n===== EQUITY CURVE =====\n\n";

        for (const auto& point : result.equityCurve) {

            std::cout << point.timestamp
                      << " | Equity: "
                      << point.equity
                      << '\n';
        }

        // ================================================
        // Trade results
        // ================================================
        std::cout << "\nTrades: "
                  << result.trades.size()
                  << "\n\n";

        for (std::size_t i = 0;
             i < result.trades.size();
             ++i) {

            const auto& trade = result.trades[i];

            std::cout << "Trade "
                      << i + 1
                      << '\n';

            std::cout << "  Entry       : "
                      << trade.entryTime
                      << " @ "
                      << trade.entryPrice
                      << '\n';

            std::cout << "  Exit        : "
                      << trade.exitTime
                      << " @ "
                      << trade.exitPrice
                      << '\n';

            std::cout << "  Quantity    : "
                      << trade.quantity
                      << '\n';

            std::cout << "  Entry Fee   : "
                      << trade.entryFee
                      << '\n';

            std::cout << "  Exit Fee    : "
                      << trade.exitFee
                      << '\n';

            std::cout << "  Gross P&L   : "
                      << trade.grossProfitLoss
                      << '\n';

            std::cout << "  Total Fees  : "
                      << trade.totalFees
                      << '\n';

            std::cout << "  Net P&L     : "
                      << trade.profitLoss
                      << "\n\n";
        }
            std::cout << "Maximum Drawdown : "
                      << result.maximumDrawdown
                      << '\n';

            std::cout << "Maximum Drawdown % : "
                      << result.maximumDrawdownPercentage
                      << "%\n";

            std::cout << "Winning Trades   : "
                      << result.winningTrades
                      << '\n';

            std::cout << "Losing Trades    : "
                      << result.losingTrades
                      << '\n';

            std::cout << "Win Rate         : "
                      << result.winRate
                      << "%\n";

            std::cout << "Average Win      : "
                      << result.averageWin
                      << '\n';

            std::cout << "Average Loss     : "
                      << result.averageLoss
                      << "\n";
    }
    catch (const std::exception& e) {

        std::cerr << "Error: "
                  << e.what()
                  << '\n';

        return 1;
    }

    return 0;
}
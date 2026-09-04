#include "include/Backtester.hpp"
#include "include/Candle.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

std::vector<Candle> readCSV(const std::string& filename);

int main() {
    try {
        const auto candles =
            readCSV("data/BTCUSDT.csv");

        std::cout << "Candles loaded: "
                  << candles.size() << "\n\n";

        constexpr std::size_t fastPeriod = 2;
        constexpr std::size_t slowPeriod = 3;

        constexpr double initialCapital = 100000.0;
        constexpr double tradingFeeRate = 0.001;
        constexpr double slippageRate = 0.0005;

        const BacktestResult result =
            runBacktest(
            candles,
            fastPeriod,
            slowPeriod,
            initialCapital,
            tradingFeeRate,
            slippageRate
        );
        std::cout << std::fixed
                  << std::setprecision(2);

        std::cout << "===== BACKTEST RESULT =====\n\n";

        std::cout << "Initial Capital : "
                  << result.initialCapital << '\n';

        std::cout << "Final Capital   : "
                  << result.finalCapital << '\n';

        std::cout << "Total P&L       : "
                  << result.totalProfitLoss << "\n\n";

        std::cout << "Trades: "
                  << result.trades.size() << "\n\n";

        for (std::size_t i = 0;
             i < result.trades.size();
             ++i) {

            const auto& trade = result.trades[i];

            std::cout << "Trade " << i + 1 << '\n';

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
    }
    catch (const std::exception& e) {
        std::cerr << "Error: "
                  << e.what()
                  << '\n';

        return 1;
    }

    return 0;
}
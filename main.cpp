#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"
#include "include/Strategy.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

struct RSICandidate
{
    std::size_t fastPeriod{};
    std::size_t slowPeriod{};
    std::size_t rsiPeriod{};
    double rsiThreshold{};

    BacktestResult result;
};

bool betterCandidate(
    const RSICandidate& a,
    const RSICandidate& b
) {
    const bool aProfitable =
        a.result.totalProfitLoss > 0.0;

    const bool bProfitable =
        b.result.totalProfitLoss > 0.0;

    // Prefer profitable strategies
    if (aProfitable != bProfitable) {
        return aProfitable;
    }

    // Then higher profit factor
    if (a.result.profitFactor != b.result.profitFactor) {
        return a.result.profitFactor >
               b.result.profitFactor;
    }

    // Then higher Sharpe ratio
    if (a.result.sharpeRatio != b.result.sharpeRatio) {
        return a.result.sharpeRatio >
               b.result.sharpeRatio;
    }

    // Then lower drawdown
    if (a.result.maximumDrawdownPercentage !=
        b.result.maximumDrawdownPercentage) {

        return a.result.maximumDrawdownPercentage <
               b.result.maximumDrawdownPercentage;
    }

    // Finally prefer more trades
    return a.result.trades.size() >
           b.result.trades.size();
}

int main()
{
    // ============================================================
    // CONFIGURATION
    // ============================================================

    const std::string filename =
        "data/BTCUSDT.csv";

    const double initialCapital =
        100000.0;

    const double tradingFeeRate =
        0.001;

    const double slippageRate =
        0.0005;

    // Risk management
    const double stopLossPercentage =
        0.02;       // 2%

    const double takeProfitPercentage =
        0.04;       // 4%

    // ============================================================
    // LOAD DATA
    // ============================================================

    const auto candles =
        readCSV(filename);

    if (candles.empty()) {
        std::cerr
            << "No candles loaded.\n";

        return 1;
    }

    std::cout
        << "Candles loaded: "
        << candles.size()
        << "\n";

    std::cout
        << std::fixed
        << std::setprecision(2);

    // ============================================================
    // BASELINE SMA 2/3
    // ============================================================

    std::cout
        << "\n========================================\n";

    std::cout
        << "BASELINE SMA 2/3\n";

    std::cout
        << "========================================\n\n";

    const BacktestResult baseline =
        runBacktest(
            candles,
            2,
            3,
            initialCapital,
            tradingFeeRate,
            slippageRate
        );

    std::cout
        << "Initial Capital : "
        << initialCapital
        << "\n";

    std::cout
        << "Final Capital   : "
        << baseline.finalCapital
        << "\n";

    std::cout
        << "Total P&L       : "
        << baseline.totalProfitLoss
        << "\n";

    std::cout
        << "Trades          : "
        << baseline.trades.size()
        << "\n";

    std::cout
        << "Win Rate        : "
        << baseline.winRate
        << "%\n";

    std::cout
        << "Profit Factor   : "
        << baseline.profitFactor
        << "\n";

    std::cout
        << "Max Drawdown    : "
        << baseline.maximumDrawdownPercentage
        << "%\n";

    std::cout
        << "Sharpe Ratio    : "
        << baseline.sharpeRatio
        << "\n";

    // ============================================================
    // TRAIN / TEST SPLIT
    // ============================================================

    const std::size_t totalCandles =
        candles.size();

    const std::size_t trainEnd =
        static_cast<std::size_t>(
            totalCandles * 0.70
        ) - 1;

    const std::size_t testStart =
        trainEnd + 1;

    const std::size_t testEnd =
        totalCandles - 1;

    std::cout
        << "\n========================================\n";

    std::cout
        << "          TRAIN / TEST SPLIT\n";

    std::cout
        << "========================================\n\n";

    std::cout
        << "Train candles : 0 - "
        << trainEnd
        << " ("
        << trainEnd + 1
        << " candles)\n";

    std::cout
        << "Test candles  : "
        << testStart
        << " - "
        << testEnd
        << " ("
        << testEnd - testStart + 1
        << " candles)\n";

    std::cout
        << "Split ratio    : 70% / 30%\n";

    // ============================================================
    // RSI + SMA HYPOTHESIS SEARCH
    // ============================================================

    const std::vector<std::size_t>
        fastPeriods = {
            3, 5, 8
        };

    const std::vector<std::size_t>
        slowPeriods = {
            10, 15, 20, 30
        };

    const std::vector<std::size_t>
        rsiPeriods = {
            7, 14, 21
        };

    const std::vector<double>
        rsiThresholds = {
            50.0, 55.0, 60.0
        };

    const std::size_t minimumTrades =
        5;

    std::vector<RSICandidate>
        candidates;

    std::cout
        << "\n========================================\n";

    std::cout
        << "     RSI + SMA + RISK MANAGEMENT SEARCH\n";

    std::cout
        << "========================================\n\n";

    std::cout
        << "Stop Loss      : "
        << stopLossPercentage * 100.0
        << "%\n";

    std::cout
        << "Take Profit    : "
        << takeProfitPercentage * 100.0
        << "%\n\n";

    std::cout
        << "Fast   Slow   RSI    Threshold   "
        << "P&L           PF        DD%       "
        << "Win Rate   Sharpe     Trades\n";

    std::cout
        << "-------------------------------------------------------------------------------\n";

    // ============================================================
    // SEARCH ALL COMBINATIONS
    // ============================================================

    for (const std::size_t fast :
         fastPeriods) {

        for (const std::size_t slow :
             slowPeriods) {

            if (fast >= slow) {
                continue;
            }

            for (const std::size_t rsiPeriod :
                 rsiPeriods) {

                for (const double threshold :
                     rsiThresholds) {

                    const BacktestResult result =
                        runBacktest(
                            candles,
                            StrategyType::RSI_SMA_TREND,
                            fast,
                            slow,
                            rsiPeriod,
                            threshold,
                            stopLossPercentage,
                            takeProfitPercentage,
                            initialCapital,
                            tradingFeeRate,
                            slippageRate,
                            0,
                            trainEnd
                        );

                    // Ignore strategies with too few trades
                    if (result.trades.size() <
                        minimumTrades) {

                        continue;
                    }

                    RSICandidate candidate{
                        fast,
                        slow,
                        rsiPeriod,
                        threshold,
                        result
                    };

                    candidates.push_back(
                        candidate
                    );

                    std::cout
                        << std::left
                        << std::setw(7)
                        << fast

                        << std::setw(7)
                        << slow

                        << std::setw(7)
                        << rsiPeriod

                        << std::setw(12)
                        << threshold

                        << std::setw(14)
                        << result.totalProfitLoss

                        << std::setw(10)
                        << result.profitFactor

                        << std::setw(10)
                        << result.maximumDrawdownPercentage

                        << std::setw(11)
                        << result.winRate

                        << std::setw(11)
                        << result.sharpeRatio

                        << result.trades.size()

                        << "\n";
                }
            }
        }
    }

    // ============================================================
    // CHECK CANDIDATES
    // ============================================================

    if (candidates.empty()) {

        std::cout
            << "\nNo RSI candidates met "
            << "the minimum trade requirement.\n";

        return 0;
    }

    // ============================================================
    // SORT CANDIDATES
    // ============================================================

    std::sort(
        candidates.begin(),
        candidates.end(),
        betterCandidate
    );

    const RSICandidate& best =
        candidates.front();

    // ============================================================
    // SELECTED MODEL
    // ============================================================

    std::cout
        << "\n========================================\n";

    std::cout
        << "       SELECTED TRAINING MODEL\n";

    std::cout
        << "========================================\n\n";

    std::cout
        << "Fast SMA       : "
        << best.fastPeriod
        << "\n";

    std::cout
        << "Slow SMA       : "
        << best.slowPeriod
        << "\n";

    std::cout
        << "RSI Period     : "
        << best.rsiPeriod
        << "\n";

    std::cout
        << "RSI Threshold  : "
        << best.rsiThreshold
        << "\n";

    std::cout
        << "Stop Loss      : "
        << stopLossPercentage * 100.0
        << "%\n";

    std::cout
        << "Take Profit    : "
        << takeProfitPercentage * 100.0
        << "%\n";

    std::cout
        << "Train P&L      : "
        << best.result.totalProfitLoss
        << "\n";

    std::cout
        << "Train PF       : "
        << best.result.profitFactor
        << "\n";

    std::cout
        << "Train DD%      : "
        << best.result.maximumDrawdownPercentage
        << "\n";

    std::cout
        << "Train Win Rate : "
        << best.result.winRate
        << "%\n";

    std::cout
        << "Train Trades   : "
        << best.result.trades.size()
        << "\n";

    // ============================================================
    // OUT-OF-SAMPLE TEST
    // ============================================================

    std::cout
        << "\n========================================\n";

    std::cout
        << "          OUT-OF-SAMPLE TEST\n";

    std::cout
        << "========================================\n\n";

    const BacktestResult testResult =
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
            testEnd
        );

    std::cout
        << "Initial Capital : "
        << initialCapital
        << "\n";

    std::cout
        << "Final Capital   : "
        << testResult.finalCapital
        << "\n";

    std::cout
        << "Total P&L       : "
        << testResult.totalProfitLoss
        << "\n";

    std::cout
        << "Trades          : "
        << testResult.trades.size()
        << "\n";

    std::cout
        << "Win Rate        : "
        << testResult.winRate
        << "%\n";

    std::cout
        << "Profit Factor   : "
        << testResult.profitFactor
        << "\n";

    std::cout
        << "Max Drawdown    : "
        << testResult.maximumDrawdownPercentage
        << "%\n";

    std::cout
        << "Sharpe Ratio    : "
        << testResult.sharpeRatio
        << "\n";

    // ============================================================
    // FINAL VERDICT
    // ============================================================

    std::cout
        << "\n========================================\n";

    std::cout
        << "             FINAL VERDICT\n";

    std::cout
        << "========================================\n\n";

    std::cout
        << "Selected Model : SMA "
        << best.fastPeriod
        << "/"
        << best.slowPeriod
        << " + RSI "
        << best.rsiPeriod
        << " @ "
        << best.rsiThreshold
        << "\n";

    std::cout
        << "Stop Loss      : "
        << stopLossPercentage * 100.0
        << "%\n";

    std::cout
        << "Take Profit    : "
        << takeProfitPercentage * 100.0
        << "%\n";

    std::cout
        << "Train P&L      : "
        << best.result.totalProfitLoss
        << "\n";

    std::cout
        << "Test P&L       : "
        << testResult.totalProfitLoss
        << "\n";

    std::cout
        << "Test PF        : "
        << testResult.profitFactor
        << "\n";

    std::cout
        << "Test Win Rate  : "
        << testResult.winRate
        << "%\n";

    if (
        testResult.totalProfitLoss > 0.0 &&
        testResult.profitFactor > 1.0 &&
        testResult.trades.size() >= 5
    ) {

        std::cout
            << "\nSTATUS: PROFITABLE "
            << "OUT-OF-SAMPLE\n";

        std::cout
            << "Further validation is required "
            << "before production use.\n";
    }
    else {

        std::cout
            << "\nSTATUS: NOT PROFITABLE "
            << "OUT-OF-SAMPLE\n";

        std::cout
            << "Do not treat this model as "
            << "production-ready.\n";
    }

    std::cout
        << "\n========================================\n";

    std::cout
        << "              DONE\n";

    std::cout
        << "========================================\n";

    return 0;
}
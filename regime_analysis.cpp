#include "include/Backtester.hpp"
#include "include/CSVReader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct RegimeStats
{
    std::string name;

    std::size_t candles{};
    std::size_t trades{};
    std::size_t winningTrades{};
    std::size_t losingTrades{};

    double profitLoss{};
    double profitFactor{};
    double expectancy{};
    double winRate{};
    double maximumDrawdownPercentage{};
};


// =========================================================
// REGIME CLASSIFICATION
// =========================================================

enum class MarketRegime
{
    STRONG_UPTREND,
    WEAK_UPTREND,
    SIDEWAYS,
    WEAK_DOWNTREND,
    STRONG_DOWNTREND
};


MarketRegime classifyTrend(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t lookback)
{
    if (index < lookback)
    {
        return MarketRegime::SIDEWAYS;
    }

    const double oldPrice =
        candles[index - lookback].close;

    const double currentPrice =
        candles[index].close;

    if (oldPrice <= 0.0)
    {
        return MarketRegime::SIDEWAYS;
    }

    const double changePercentage =
        (
            (currentPrice / oldPrice)
            - 1.0
        ) * 100.0;

    /*
        Trend classification over the lookback period.

        >= +10% : strong uptrend
        >= +3%  : weak uptrend
        > -3%   : sideways
        > -10%  : weak downtrend
        <= -10% : strong downtrend
    */

    if (changePercentage >= 10.0)
    {
        return MarketRegime::STRONG_UPTREND;
    }

    if (changePercentage >= 3.0)
    {
        return MarketRegime::WEAK_UPTREND;
    }

    if (changePercentage > -3.0)
    {
        return MarketRegime::SIDEWAYS;
    }

    if (changePercentage > -10.0)
    {
        return MarketRegime::WEAK_DOWNTREND;
    }

    return MarketRegime::STRONG_DOWNTREND;
}


// =========================================================
// REGIME NAME
// =========================================================

std::string regimeName(
    MarketRegime regime)
{
    switch (regime)
    {
        case MarketRegime::STRONG_UPTREND:
            return "Strong Uptrend";

        case MarketRegime::WEAK_UPTREND:
            return "Weak Uptrend";

        case MarketRegime::SIDEWAYS:
            return "Sideways";

        case MarketRegime::WEAK_DOWNTREND:
            return "Weak Downtrend";

        case MarketRegime::STRONG_DOWNTREND:
            return "Strong Downtrend";
    }

    return "Unknown";
}


// =========================================================
// ATR
// =========================================================

double calculateATR(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t period)
{
    if (index == 0 ||
        index < period)
    {
        return 0.0;
    }

    double trueRangeSum = 0.0;

    const std::size_t start =
        index - period + 1;

    for (std::size_t i = start;
         i <= index;
         ++i)
    {
        const double previousClose =
            candles[i - 1].close;

        const double highLow =
            candles[i].high -
            candles[i].low;

        const double highPreviousClose =
            std::abs(
                candles[i].high -
                previousClose
            );

        const double lowPreviousClose =
            std::abs(
                candles[i].low -
                previousClose
            );

        const double trueRange =
            std::max(
                {
                    highLow,
                    highPreviousClose,
                    lowPreviousClose
                }
            );

        trueRangeSum += trueRange;
    }

    return trueRangeSum /
           static_cast<double>(period);
}


// =========================================================
// VOLATILITY CLASSIFICATION
// =========================================================

std::string classifyVolatility(
    const std::vector<Candle>& candles,
    std::size_t index,
    std::size_t atrPeriod)
{
    const double atr =
        calculateATR(
            candles,
            index,
            atrPeriod
        );

    if (atr <= 0.0 ||
        candles[index].close <= 0.0)
    {
        return "Unknown";
    }

    const double atrPercentage =
        (
            atr /
            candles[index].close
        ) * 100.0;

    /*
        Approximate hourly BTC volatility buckets.

        < 0.30% : Low
        < 0.70% : Normal
        >=0.70% : High
    */

    if (atrPercentage < 0.30)
    {
        return "Low Volatility";
    }

    if (atrPercentage < 0.70)
    {
        return "Normal Volatility";
    }

    return "High Volatility";
}


// =========================================================
// CALCULATE DRAWDOWN
// =========================================================

double calculateMaximumDrawdown(
    const std::vector<EquityPoint>& curve,
    double initialCapital)
{
    double peak =
        initialCapital;

    double maximumDrawdown =
        0.0;

    for (const auto& point :
         curve)
    {
        if (point.equity > peak)
        {
            peak =
                point.equity;
        }

        if (peak > 0.0)
        {
            const double drawdown =
                (
                    (peak - point.equity)
                    / peak
                ) * 100.0;

            maximumDrawdown =
                std::max(
                    maximumDrawdown,
                    drawdown
                );
        }
    }

    return maximumDrawdown;
}


// =========================================================
// CALCULATE STATISTICS FOR A SET OF TRADES
// =========================================================

RegimeStats calculateTradeStats(
    const std::string& name,
    const std::vector<Trade>& trades)
{
    RegimeStats stats;

    stats.name =
        name;

    stats.trades =
        trades.size();

    double winningProfit =
        0.0;

    double losingProfit =
        0.0;

    for (const auto& trade :
         trades)
    {
        if (trade.profitLoss > 0.0)
        {
            winningProfit +=
                trade.profitLoss;

            ++stats.winningTrades;
        }
        else if (trade.profitLoss < 0.0)
        {
            losingProfit +=
                trade.profitLoss;
        }
    }

    stats.losingTrades =
        stats.trades -
        stats.winningTrades;

    stats.profitLoss =
        winningProfit +
        losingProfit;

    if (stats.trades > 0)
    {
        stats.winRate =
            (
                static_cast<double>(
                    stats.winningTrades
                )
                /
                static_cast<double>(
                    stats.trades
                )
            ) * 100.0;
    }

    double averageWin =
        0.0;

    double averageLoss =
        0.0;

    if (stats.winningTrades > 0)
    {
        averageWin =
            winningProfit /
            static_cast<double>(
                stats.winningTrades
            );
    }

    if (stats.losingTrades > 0)
    {
        averageLoss =
            losingProfit /
            static_cast<double>(
                stats.losingTrades
            );
    }

    const double winProbability =
        stats.trades > 0
            ? static_cast<double>(
                  stats.winningTrades
              )
              /
              static_cast<double>(
                  stats.trades
              )
            : 0.0;

    const double lossProbability =
        1.0 -
        winProbability;

    stats.expectancy =
        (
            winProbability *
            averageWin
        )
        +
        (
            lossProbability *
            averageLoss
        );

    if (losingProfit < 0.0)
    {
        stats.profitFactor =
            winningProfit /
            (-losingProfit);
    }
    else if (winningProfit > 0.0)
    {
        stats.profitFactor =
            999999.0;
    }
    else
    {
        stats.profitFactor =
            0.0;
    }

    return stats;
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

    // Current baseline strategy.
    const std::size_t fastPeriod =
        3;

    const std::size_t slowPeriod =
        20;

    const std::size_t rsiPeriod =
        21;

    const double rsiThreshold =
        60.0;

    // Same six OOS windows.
    const std::vector<
        std::pair<std::size_t, std::size_t>
    > oosWindows =
    {
        {4320, 5039},
        {5040, 5759},
        {5760, 6479},
        {6480, 7199},
        {7200, 7919},
        {7920, 8639}
    };

    const std::size_t trendLookback =
        168;

    const std::size_t atrPeriod =
        14;

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
        << "         MARKET REGIME ANALYSIS\n"
        << "========================================\n";

    std::cout
        << "Strategy: SMA "
        << fastPeriod
        << "/"
        << slowPeriod
        << " + RSI "
        << rsiPeriod
        << " @ "
        << rsiThreshold
        << '\n';

    std::cout
        << "Fee      : "
        << fee * 100.0
        << "%\n";

    std::cout
        << "Slippage : "
        << slippage * 100.0
        << "%\n";

    std::cout
        << "Trend Lookback : "
        << trendLookback
        << " candles\n";

    // =========================================================
    // STORAGE
    // =========================================================

    std::vector<Trade> tradesByTrend[5];

    std::size_t regimeCandleCount[5] = {
        0, 0, 0, 0, 0
    };

    // =========================================================
    // RUN STRATEGY ON ALL OOS WINDOWS
    // =========================================================

    for (const auto& window :
         oosWindows)
    {
        const BacktestResult result =
            runBacktest(
                candles,
                StrategyType::RSI_SMA_TREND,

                fastPeriod,
                slowPeriod,
                rsiPeriod,
                rsiThreshold,

                stopLossPercentage,
                takeProfitPercentage,

                initialCapital,
                fee,
                slippage,

                window.first,
                window.second
            );

        // -----------------------------------------------------
        // Count candles by trend regime.
        // -----------------------------------------------------

        for (std::size_t i = window.first;
             i <= window.second;
             ++i)
        {
            if (i < trendLookback)
            {
                continue;
            }

            const MarketRegime regime =
                classifyTrend(
                    candles,
                    i,
                    trendLookback
                );

            ++regimeCandleCount[
                static_cast<int>(regime)
            ];
        }

        // -----------------------------------------------------
        // Assign each completed trade to the regime at entry.
        // -----------------------------------------------------

        for (const auto& trade :
             result.trades)
        {
            std::size_t entryIndex =
                window.first;

            for (std::size_t i =
                     window.first;
                 i <= window.second;
                 ++i)
            {
                if (candles[i].timestamp ==
                    trade.entryTime)
                {
                    entryIndex =
                        i;

                    break;
                }
            }

            if (entryIndex <
                trendLookback)
            {
                continue;
            }

            const MarketRegime regime =
                classifyTrend(
                    candles,
                    entryIndex,
                    trendLookback
                );

            tradesByTrend[
                static_cast<int>(regime)
            ].push_back(trade);
        }
    }

    // =========================================================
    // TREND REGIME RESULTS
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "          TREND REGIME RESULTS\n"
        << "========================================\n";

    std::cout
        << "\nRegime              Candles   Trades"
        << "    P&L       PF       Expectancy"
        << "      Win Rate\n";

    std::cout
        << "------------------------------------------------------------\n";

    std::vector<RegimeStats> trendResults;

    for (int i = 0; i < 5; ++i)
    {
        const MarketRegime regime =
            static_cast<MarketRegime>(i);

        RegimeStats stats =
            calculateTradeStats(
                regimeName(regime),
                tradesByTrend[i]
            );

        stats.candles =
            regimeCandleCount[i];

        trendResults.push_back(
            stats
        );

        std::cout
            << std::left
            << std::setw(20)
            << stats.name

            << std::right
            << std::setw(8)
            << stats.candles

            << std::setw(8)
            << stats.trades

            << std::setw(11)
            << std::fixed
            << std::setprecision(2)
            << stats.profitLoss

            << std::setw(9)
            << stats.profitFactor

            << std::setw(14)
            << stats.expectancy

            << std::setw(12)
            << stats.winRate
            << "%\n";
    }

    // =========================================================
    // VOLATILITY ANALYSIS
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "        VOLATILITY REGIME ANALYSIS\n"
        << "========================================\n";

    std::vector<Trade> lowVolatilityTrades;
    std::vector<Trade> normalVolatilityTrades;
    std::vector<Trade> highVolatilityTrades;

    for (const auto& window :
         oosWindows)
    {
        const BacktestResult result =
            runBacktest(
                candles,
                StrategyType::RSI_SMA_TREND,

                fastPeriod,
                slowPeriod,
                rsiPeriod,
                rsiThreshold,

                stopLossPercentage,
                takeProfitPercentage,

                initialCapital,
                fee,
                slippage,

                window.first,
                window.second
            );

        for (const auto& trade :
             result.trades)
        {
            std::size_t entryIndex =
                window.first;

            for (std::size_t i =
                     window.first;
                 i <= window.second;
                 ++i)
            {
                if (candles[i].timestamp ==
                    trade.entryTime)
                {
                    entryIndex =
                        i;

                    break;
                }
            }

            const std::string volatility =
                classifyVolatility(
                    candles,
                    entryIndex,
                    atrPeriod
                );

            if (volatility ==
                "Low Volatility")
            {
                lowVolatilityTrades.push_back(
                    trade
                );
            }
            else if (volatility ==
                     "Normal Volatility")
            {
                normalVolatilityTrades.push_back(
                    trade
                );
            }
            else if (volatility ==
                     "High Volatility")
            {
                highVolatilityTrades.push_back(
                    trade
                );
            }
        }
    }

    const std::vector<RegimeStats>
        volatilityResults =
    {
        calculateTradeStats(
            "Low Volatility",
            lowVolatilityTrades
        ),

        calculateTradeStats(
            "Normal Volatility",
            normalVolatilityTrades
        ),

        calculateTradeStats(
            "High Volatility",
            highVolatilityTrades
        )
    };

    std::cout
        << "\nRegime              Trades      P&L"
        << "       PF       Expectancy"
        << "      Win Rate\n";

    std::cout
        << "------------------------------------------------------------\n";

    for (const auto& stats :
         volatilityResults)
    {
        std::cout
            << std::left
            << std::setw(20)
            << stats.name

            << std::right
            << std::setw(7)
            << stats.trades

            << std::setw(12)
            << stats.profitLoss

            << std::setw(9)
            << stats.profitFactor

            << std::setw(14)
            << stats.expectancy

            << std::setw(12)
            << stats.winRate
            << "%\n";
    }

    // =========================================================
    // SAVE TREND RESULTS
    // =========================================================

    std::ofstream trendFile(
        "results/trend_regime_analysis.csv"
    );

    if (trendFile)
    {
        trendFile
            << "regime,candles,trades,profit_loss,"
            << "profit_factor,expectancy,win_rate\n";

        for (const auto& stats :
             trendResults)
        {
            trendFile
                << stats.name
                << ","
                << stats.candles
                << ","
                << stats.trades
                << ","
                << stats.profitLoss
                << ","
                << stats.profitFactor
                << ","
                << stats.expectancy
                << ","
                << stats.winRate
                << '\n';
        }

        trendFile.close();
    }

    // =========================================================
    // SAVE VOLATILITY RESULTS
    // =========================================================

    std::ofstream volatilityFile(
        "results/volatility_regime_analysis.csv"
    );

    if (volatilityFile)
    {
        volatilityFile
            << "regime,trades,profit_loss,"
            << "profit_factor,expectancy,win_rate\n";

        for (const auto& stats :
             volatilityResults)
        {
            volatilityFile
                << stats.name
                << ","
                << stats.trades
                << ","
                << stats.profitLoss
                << ","
                << stats.profitFactor
                << ","
                << stats.expectancy
                << ","
                << stats.winRate
                << '\n';
        }

        volatilityFile.close();
    }

    // =========================================================
    // DONE
    // =========================================================

    std::cout
        << "\n========================================\n"
        << "              FILES SAVED\n"
        << "========================================\n";

    std::cout
        << "results/trend_regime_analysis.csv\n";

    std::cout
        << "results/volatility_regime_analysis.csv\n";

    std::cout
        << "\n========================================\n"
        << "                 DONE\n"
        << "========================================\n";

    return 0;
}
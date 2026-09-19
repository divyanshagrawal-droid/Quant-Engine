#pragma once

#include "Candle.hpp"
#include "Backtester.hpp"

#include <cstddef>
#include <vector>

struct WalkForwardResult
{
    // ------------------------------------------------------------------------
    // Window information
    // ------------------------------------------------------------------------

    std::size_t trainStart{};
    std::size_t trainEnd{};

    std::size_t testStart{};
    std::size_t testEnd{};


    // ------------------------------------------------------------------------
    // Selected strategy parameters
    // ------------------------------------------------------------------------

    std::size_t fastPeriod{};
    std::size_t slowPeriod{};

    std::size_t rsiPeriod{};

    double rsiBuyThreshold{};


    // ------------------------------------------------------------------------
    // Training-derived volatility filter
    // ------------------------------------------------------------------------

    std::size_t atrPeriod{};

    /*
     * Maximum ATR percentage allowed for a new BUY.
     *
     * This value is calculated ONLY from the training window
     * and then frozen before the test window begins.
     */

    double maxAtrPercentage{};


    // ------------------------------------------------------------------------
    // Out-of-sample performance
    // ------------------------------------------------------------------------

    double testProfitLoss{};

    double testFinalCapital{};

    double testMaximumDrawdownPercentage{};

    double testWinRate{};

    double testProfitFactor{};

    double testSharpeRatio{};

    std::size_t testTrades{};


    // ------------------------------------------------------------------------
    // OOS equity curve
    // ------------------------------------------------------------------------

    std::vector<EquityPoint> testEquityCurve;
};


// ============================================================================
// WALK-FORWARD VALIDATION
// ============================================================================

std::vector<WalkForwardResult> runWalkForwardValidation(
    const std::vector<Candle>& candles,

    std::size_t trainSize,

    std::size_t testSize,

    const std::vector<std::size_t>& fastPeriods,

    const std::vector<std::size_t>& slowPeriods,

    const std::vector<std::size_t>& rsiPeriods,

    const std::vector<double>& rsiThresholds,

    double stopLossPercentage,

    double takeProfitPercentage,

    double initialCapital,

    double tradingFeeRate,

    double slippageRate
);
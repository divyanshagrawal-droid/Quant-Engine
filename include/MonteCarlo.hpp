#pragma once

#include <cstddef>
#include <vector>

struct MonteCarloResult
{
    std::size_t simulations{};
    std::size_t tradesPerSimulation{};

    double observedFinalCapital{};
    double observedReturnPercentage{};
    double observedMaximumDrawdownPercentage{};

    double medianFinalCapital{};
    double percentile5FinalCapital{};
    double percentile95FinalCapital{};

    double medianReturnPercentage{};
    double percentile5ReturnPercentage{};
    double percentile95ReturnPercentage{};

    double medianMaximumDrawdownPercentage{};
    double percentile95MaximumDrawdownPercentage{};
    double worstMaximumDrawdownPercentage{};

    double probabilityOfProfit{};
    double probabilityOfLoss{};
    double probabilityDrawdownAbove10{};
    double probabilityDrawdownAbove15{};
    double probabilityDrawdownAbove20{};

    double meanFinalCapital{};
    double meanReturnPercentage{};
};


MonteCarloResult runMonteCarlo(
    const std::vector<double>& tradeReturns,
    double initialCapital,
    std::size_t simulations,
    unsigned int seed = 42
);
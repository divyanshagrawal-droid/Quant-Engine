#include "../include/MonteCarlo.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <random>
#include <vector>


namespace
{

// ============================================================
// PERCENTILE
// ============================================================

double calculatePercentile(
    std::vector<double> values,
    double percentile)
{
    if (values.empty())
    {
        return 0.0;
    }

    std::sort(
        values.begin(),
        values.end()
    );

    if (values.size() == 1)
    {
        return values.front();
    }

    const double position =
        (percentile / 100.0) *
        static_cast<double>(values.size() - 1);

    const std::size_t lower =
        static_cast<std::size_t>(
            std::floor(position)
        );

    const std::size_t upper =
        static_cast<std::size_t>(
            std::ceil(position)
        );

    if (lower == upper)
    {
        return values[lower];
    }

    const double weight =
        position -
        static_cast<double>(lower);

    return
        values[lower] * (1.0 - weight)
        +
        values[upper] * weight;
}


// ============================================================
// MAXIMUM DRAWDOWN FROM TRADE RETURNS
// ============================================================

double calculateMaximumDrawdown(
    const std::vector<double>& returns,
    double initialCapital)
{
    double capital = initialCapital;
    double peak = initialCapital;
    double maximumDrawdownPercentage = 0.0;

    for (double tradeReturn : returns)
    {
        capital *= (1.0 + tradeReturn);

        if (capital > peak)
        {
            peak = capital;
        }

        if (peak > 0.0)
        {
            const double drawdown =
                ((peak - capital) / peak) * 100.0;

            maximumDrawdownPercentage =
                std::max(
                    maximumDrawdownPercentage,
                    drawdown
                );
        }
    }

    return maximumDrawdownPercentage;
}

}


// ============================================================
// MONTE CARLO
// ============================================================

MonteCarloResult runMonteCarlo(
    const std::vector<double>& tradeReturns,
    double initialCapital,
    std::size_t simulations,
    unsigned int seed)
{
    MonteCarloResult result;

    result.simulations = simulations;
    result.tradesPerSimulation =
        tradeReturns.size();

    if (tradeReturns.empty() ||
        simulations == 0 ||
        initialCapital <= 0.0)
    {
        return result;
    }


    // ========================================================
    // OBSERVED RESULT
    // ========================================================

    double observedCapital =
        initialCapital;

    for (double tradeReturn : tradeReturns)
    {
        observedCapital *=
            (1.0 + tradeReturn);
    }

    result.observedFinalCapital =
        observedCapital;

    result.observedReturnPercentage =
        (
            (observedCapital / initialCapital)
            - 1.0
        ) * 100.0;

    result.observedMaximumDrawdownPercentage =
        calculateMaximumDrawdown(
            tradeReturns,
            initialCapital
        );


    // ========================================================
    // RANDOM NUMBER GENERATOR
    // ========================================================

    std::mt19937 generator(seed);

    std::uniform_int_distribution<std::size_t>
        distribution(
            0,
            tradeReturns.size() - 1
        );


    // ========================================================
    // STORAGE
    // ========================================================

    std::vector<double> finalCapitals;
    std::vector<double> returns;
    std::vector<double> drawdowns;

    finalCapitals.reserve(simulations);
    returns.reserve(simulations);
    drawdowns.reserve(simulations);


    // ========================================================
    // BOOTSTRAP SIMULATIONS
    // ========================================================

    for (std::size_t simulation = 0;
         simulation < simulations;
         ++simulation)
    {
        double capital =
            initialCapital;

        double peak =
            initialCapital;

        double maximumDrawdown =
            0.0;


        // Sample the same number of trades
        // as the real OOS sample.
        for (std::size_t trade = 0;
             trade < tradeReturns.size();
             ++trade)
        {
            const double selectedReturn =
                tradeReturns[
                    distribution(generator)
                ];

            capital *=
                (1.0 + selectedReturn);


            if (capital > peak)
            {
                peak = capital;
            }

            if (peak > 0.0)
            {
                const double drawdown =
                    (
                        (peak - capital)
                        / peak
                    ) * 100.0;

                maximumDrawdown =
                    std::max(
                        maximumDrawdown,
                        drawdown
                    );
            }
        }


        const double simulationReturn =
            (
                (capital / initialCapital)
                - 1.0
            ) * 100.0;


        finalCapitals.push_back(
            capital
        );

        returns.push_back(
            simulationReturn
        );

        drawdowns.push_back(
            maximumDrawdown
        );
    }


    // ========================================================
    // FINAL CAPITAL STATISTICS
    // ========================================================

    result.percentile5FinalCapital =
        calculatePercentile(
            finalCapitals,
            5.0
        );

    result.medianFinalCapital =
        calculatePercentile(
            finalCapitals,
            50.0
        );

    result.percentile95FinalCapital =
        calculatePercentile(
            finalCapitals,
            95.0
        );


    // ========================================================
    // RETURN STATISTICS
    // ========================================================

    result.percentile5ReturnPercentage =
        calculatePercentile(
            returns,
            5.0
        );

    result.medianReturnPercentage =
        calculatePercentile(
            returns,
            50.0
        );

    result.percentile95ReturnPercentage =
        calculatePercentile(
            returns,
            95.0
        );


    // ========================================================
    // DRAWDOWN STATISTICS
    // ========================================================

    result.medianMaximumDrawdownPercentage =
        calculatePercentile(
            drawdowns,
            50.0
        );

    result.percentile95MaximumDrawdownPercentage =
        calculatePercentile(
            drawdowns,
            95.0
        );

    result.worstMaximumDrawdownPercentage =
        *std::max_element(
            drawdowns.begin(),
            drawdowns.end()
        );


    // ========================================================
    // MEANS
    // ========================================================

    result.meanFinalCapital =
        std::accumulate(
            finalCapitals.begin(),
            finalCapitals.end(),
            0.0
        )
        /
        static_cast<double>(
            finalCapitals.size()
        );

    result.meanReturnPercentage =
        std::accumulate(
            returns.begin(),
            returns.end(),
            0.0
        )
        /
        static_cast<double>(
            returns.size()
        );


    // ========================================================
    // PROBABILITIES
    // ========================================================

    std::size_t profitableSimulations = 0;
    std::size_t losingSimulations = 0;

    std::size_t drawdownAbove10 = 0;
    std::size_t drawdownAbove15 = 0;
    std::size_t drawdownAbove20 = 0;


    for (std::size_t i = 0;
         i < simulations;
         ++i)
    {
        if (returns[i] > 0.0)
        {
            ++profitableSimulations;
        }
        else if (returns[i] < 0.0)
        {
            ++losingSimulations;
        }

        if (drawdowns[i] > 10.0)
        {
            ++drawdownAbove10;
        }

        if (drawdowns[i] > 15.0)
        {
            ++drawdownAbove15;
        }

        if (drawdowns[i] > 20.0)
        {
            ++drawdownAbove20;
        }
    }


    result.probabilityOfProfit =
        (
            static_cast<double>(
                profitableSimulations
            )
            /
            static_cast<double>(
                simulations
            )
        ) * 100.0;


    result.probabilityOfLoss =
        (
            static_cast<double>(
                losingSimulations
            )
            /
            static_cast<double>(
                simulations
            )
        ) * 100.0;


    result.probabilityDrawdownAbove10 =
        (
            static_cast<double>(
                drawdownAbove10
            )
            /
            static_cast<double>(
                simulations
            )
        ) * 100.0;


    result.probabilityDrawdownAbove15 =
        (
            static_cast<double>(
                drawdownAbove15
            )
            /
            static_cast<double>(
                simulations
            )
        ) * 100.0;


    result.probabilityDrawdownAbove20 =
        (
            static_cast<double>(
                drawdownAbove20
            )
            /
            static_cast<double>(
                simulations
            )
        ) * 100.0;


    return result;
}
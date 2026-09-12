#include "include/DataConverter.hpp"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    const std::string outputFile =
        "data/BTCUSDT.csv";

    const std::vector<std::string> inputFiles = {
        "data/BTCUSDT-1h-2025-01.csv",
        "data/BTCUSDT-1h-2025-02.csv",
        "data/BTCUSDT-1h-2025-03.csv",
        "data/BTCUSDT-1h-2025-04.csv",
        "data/BTCUSDT-1h-2025-05.csv",
        "data/BTCUSDT-1h-2025-06.csv",
        "data/BTCUSDT-1h-2025-07.csv",
        "data/BTCUSDT-1h-2025-08.csv",
        "data/BTCUSDT-1h-2025-09.csv",
        "data/BTCUSDT-1h-2025-10.csv",
        "data/BTCUSDT-1h-2025-11.csv",
        "data/BTCUSDT-1h-2025-12.csv"
    };

    std::cout
        << "========================================\n"
        << "     QUANT ENGINE DATA CONVERTER\n"
        << "========================================\n";

    if (!convertBinanceCSV(
            inputFiles,
            outputFile))
    {
        std::cerr
            << "\nConversion failed.\n";

        return 1;
    }

    return 0;
}
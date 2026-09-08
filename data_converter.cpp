#include "include/DataConverter.hpp"

#include <iostream>
#include <string>

int main()
{
    const std::string inputFile =
        "data/BTCUSDT-1h-2025-01.csv";

    const std::string outputFile =
        "data/BTCUSDT.csv";

    if (!convertBinanceCSV(inputFile, outputFile))
    {
        std::cerr
            << "Data conversion failed.\n";

        return 1;
    }

    return 0;
}
#include "../include/Candle.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<Candle> readCSV(const std::string& filename)
{
    std::vector<Candle> candles;

    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Error: Could not open CSV file: "
                  << filename << '\n';

        return candles;
    }

    std::string line;

    // =========================================================
    // READ HEADER
    // =========================================================

    if (!std::getline(file, line))
    {
        std::cerr << "Error: CSV file is empty.\n";
        return candles;
    }

    // Remove possible carriage return from Windows files.
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    // Expected:
    // timestamp,open,high,low,close,volume

    std::stringstream headerStream(line);

    std::string column;

    std::vector<std::string> headers;

    while (std::getline(headerStream, column, ','))
    {
        headers.push_back(column);
    }

    if (headers.size() != 6 ||
        headers[0] != "timestamp" ||
        headers[1] != "open" ||
        headers[2] != "high" ||
        headers[3] != "low" ||
        headers[4] != "close" ||
        headers[5] != "volume")
    {
        std::cerr << "Error: Invalid CSV header.\n";
        std::cerr << "Expected:\n";
        std::cerr << "timestamp,open,high,low,close,volume\n";

        return candles;
    }

    // =========================================================
    // READ DATA
    // =========================================================

    std::size_t lineNumber = 1;

    while (std::getline(file, line))
    {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        // Ignore completely empty lines.
        if (line.empty())
        {
            continue;
        }

        std::stringstream stream(line);

        std::string timestamp;
        std::string openString;
        std::string highString;
        std::string lowString;
        std::string closeString;
        std::string volumeString;

        // =====================================================
        // PARSE COLUMNS
        // =====================================================

        if (!std::getline(stream, timestamp, ',') ||
            !std::getline(stream, openString, ',') ||
            !std::getline(stream, highString, ',') ||
            !std::getline(stream, lowString, ',') ||
            !std::getline(stream, closeString, ',') ||
            !std::getline(stream, volumeString, ','))
        {
            std::cerr << "Warning: Skipping malformed row at line "
                      << lineNumber << ".\n";

            continue;
        }

        try
        {
            const double open =
                std::stod(openString);

            const double high =
                std::stod(highString);

            const double low =
                std::stod(lowString);

            const double close =
                std::stod(closeString);

            const double volume =
                std::stod(volumeString);

            // =================================================
            // BASIC DATA VALIDATION
            // =================================================

            if (timestamp.empty())
            {
                std::cerr << "Warning: Skipping row at line "
                          << lineNumber
                          << " because timestamp is empty.\n";

                continue;
            }

            if (open <= 0.0 ||
                high <= 0.0 ||
                low <= 0.0 ||
                close <= 0.0 ||
                volume < 0.0)
            {
                std::cerr << "Warning: Skipping invalid market data "
                          << "at line "
                          << lineNumber
                          << ".\n";

                continue;
            }

            // OHLC consistency check.
            if (high < low ||
                high < open ||
                high < close ||
                low > open ||
                low > close)
            {
                std::cerr << "Warning: Skipping invalid OHLC data "
                          << "at line "
                          << lineNumber
                          << ".\n";

                continue;
            }

            // =================================================
            // CREATE CANDLE
            // =================================================

            Candle candle;

            candle.timestamp = timestamp;
            candle.open = open;
            candle.high = high;
            candle.low = low;
            candle.close = close;
            candle.volume = volume;

            candles.push_back(candle);
        }
        catch (const std::exception&)
        {
            std::cerr << "Warning: Skipping invalid numeric data "
                      << "at line "
                      << lineNumber
                      << ".\n";

            continue;
        }
    }

    file.close();

    // =========================================================
    // SORT CHRONOLOGICALLY
    // =========================================================

    std::sort(
        candles.begin(),
        candles.end(),
        [](const Candle& a, const Candle& b)
        {
            return a.timestamp < b.timestamp;
        }
    );

    // =========================================================
    // FINAL STATUS
    // =========================================================

    std::cout << "CSV loaded successfully: "
              << candles.size()
              << " valid candles.\n";

    return candles;
}
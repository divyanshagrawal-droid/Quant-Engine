#include "../include/DataConverter.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string convertTimestamp(const std::string& timestamp)
    {
        try
        {
            // Binance timestamp is in microseconds.
            const long long microseconds =
                std::stoll(timestamp);

            const std::time_t seconds =
                static_cast<std::time_t>(
                    microseconds / 1'000'000
                );

            std::tm* utcTime =
                std::gmtime(&seconds);

            if (utcTime == nullptr)
            {
                return "";
            }

            std::ostringstream output;

            output << std::put_time(
                utcTime,
                "%Y-%m-%d %H:%M:%S"
            );

            return output.str();
        }
        catch (const std::exception&)
        {
            return "";
        }
    }

    bool isHeader(const std::string& line)
    {
        if (line.empty())
        {
            return false;
        }

        // Binance raw data normally has no header.
        // If the first field cannot be converted to
        // a number, we assume it is a header.
        const std::size_t commaPosition =
            line.find(',');

        if (commaPosition == std::string::npos)
        {
            return false;
        }

        const std::string firstField =
            line.substr(0, commaPosition);

        try
        {
            std::stoll(firstField);
            return false;
        }
        catch (const std::exception&)
        {
            return true;
        }
    }
}

bool convertBinanceCSV(
    const std::string& inputFile,
    const std::string& outputFile
)
{
    std::ifstream input(inputFile);

    if (!input.is_open())
    {
        std::cerr
            << "Error: Could not open input file: "
            << inputFile
            << '\n';

        return false;
    }

    std::ofstream output(
        outputFile,
        std::ios::trunc
    );

    if (!output.is_open())
    {
        std::cerr
            << "Error: Could not create output file: "
            << outputFile
            << '\n';

        return false;
    }

    // =========================================================
    // WRITE QUANT ENGINE CSV HEADER
    // =========================================================

    output
        << "timestamp,open,high,low,close,volume\n";

    std::string line;

    std::size_t lineNumber = 0;
    std::size_t convertedRows = 0;
    std::size_t skippedRows = 0;

    bool firstLine = true;

    // =========================================================
    // READ BINANCE CSV
    // =========================================================

    while (std::getline(input, line))
    {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty())
        {
            continue;
        }

        // Skip header if the file contains one.
        if (firstLine && isHeader(line))
        {
            firstLine = false;
            continue;
        }

        firstLine = false;

        std::stringstream stream(line);

        std::vector<std::string> columns;

        std::string field;

        while (std::getline(stream, field, ','))
        {
            columns.push_back(field);
        }

        // Binance kline data should contain 12 columns.
        if (columns.size() < 12)
        {
            std::cerr
                << "Warning: Skipping malformed row at line "
                << lineNumber
                << ". Expected 12 columns, found "
                << columns.size()
                << ".\n";

            ++skippedRows;
            continue;
        }

        try
        {
            // =================================================
            // BINANCE COLUMN MAPPING
            // =================================================

            // 0 = Open time
            // 1 = Open
            // 2 = High
            // 3 = Low
            // 4 = Close
            // 5 = Volume
            // 6 = Close time
            // 7 = Quote asset volume
            // 8 = Number of trades
            // 9 = Taker buy base volume
            // 10 = Taker buy quote volume
            // 11 = Ignore

            const std::string timestamp =
                convertTimestamp(columns[0]);

            const double open =
                std::stod(columns[1]);

            const double high =
                std::stod(columns[2]);

            const double low =
                std::stod(columns[3]);

            const double close =
                std::stod(columns[4]);

            const double volume =
                std::stod(columns[5]);

            // =================================================
            // VALIDATION
            // =================================================

            if (timestamp.empty())
            {
                ++skippedRows;
                continue;
            }

            if (open <= 0.0 ||
                high <= 0.0 ||
                low <= 0.0 ||
                close <= 0.0 ||
                volume < 0.0)
            {
                std::cerr
                    << "Warning: Invalid market data at line "
                    << lineNumber
                    << ".\n";

                ++skippedRows;
                continue;
            }

            if (high < low ||
                high < open ||
                high < close ||
                low > open ||
                low > close)
            {
                std::cerr
                    << "Warning: Invalid OHLC data at line "
                    << lineNumber
                    << ".\n";

                ++skippedRows;
                continue;
            }

            // =================================================
            // WRITE CONVERTED DATA
            // =================================================

            output
                << timestamp << ','
                << std::fixed << std::setprecision(8)
                << open << ','
                << high << ','
                << low << ','
                << close << ','
                << volume
                << '\n';

            ++convertedRows;
        }
        catch (const std::exception&)
        {
            std::cerr
                << "Warning: Invalid numeric data at line "
                << lineNumber
                << ".\n";

            ++skippedRows;
        }
    }

    input.close();
    output.close();

    // =========================================================
    // SUMMARY
    // =========================================================

    std::cout
        << "\n===== DATA CONVERSION RESULT =====\n";

    std::cout
        << "Input file      : "
        << inputFile
        << '\n';

    std::cout
        << "Output file     : "
        << outputFile
        << '\n';

    std::cout
        << "Converted rows  : "
        << convertedRows
        << '\n';

    std::cout
        << "Skipped rows    : "
        << skippedRows
        << '\n';

    if (convertedRows == 0)
    {
        std::cerr
            << "Error: No valid candles were converted.\n";

        return false;
    }

    std::cout
        << "Conversion successful.\n";

    return true;
}
#include "../include/DataConverter.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct ConvertedCandle
    {
        long long timestampValue{};
        std::string timestamp;
        double open{};
        double high{};
        double low{};
        double close{};
        double volume{};
    };

    std::string convertTimestamp(const std::string& timestamp)
    {
        try
        {
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

    bool parseFile(
        const std::string& inputFile,
        std::vector<ConvertedCandle>& candles,
        std::size_t& skippedRows
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

        std::string line;
        std::size_t lineNumber = 0;
        bool firstLine = true;

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

            if (columns.size() < 12)
            {
                ++skippedRows;
                continue;
            }

            try
            {
                const long long timestampValue =
                    std::stoll(columns[0]);

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
                    ++skippedRows;
                    continue;
                }

                if (high < low ||
                    high < open ||
                    high < close ||
                    low > open ||
                    low > close)
                {
                    ++skippedRows;
                    continue;
                }

                candles.push_back({
                    timestampValue,
                    timestamp,
                    open,
                    high,
                    low,
                    close,
                    volume
                });
            }
            catch (const std::exception&)
            {
                ++skippedRows;
            }
        }

        return true;
    }
}

bool convertBinanceCSV(
    const std::vector<std::string>& inputFiles,
    const std::string& outputFile
)
{
    std::vector<ConvertedCandle> candles;

    std::size_t skippedRows = 0;

    // =========================================================
    // READ ALL MONTHLY FILES
    // =========================================================

    for (const auto& inputFile : inputFiles)
    {
        std::cout
            << "\nProcessing: "
            << inputFile
            << '\n';

        if (!parseFile(
                inputFile,
                candles,
                skippedRows))
        {
            return false;
        }
    }

    // =========================================================
    // SORT ALL CANDLES CHRONOLOGICALLY
    // =========================================================

    std::sort(
        candles.begin(),
        candles.end(),
        [](const ConvertedCandle& a,
           const ConvertedCandle& b)
        {
            return a.timestampValue <
                   b.timestampValue;
        }
    );

    // =========================================================
    // REMOVE DUPLICATES
    // =========================================================

    const auto newEnd =
        std::unique(
            candles.begin(),
            candles.end(),
            [](const ConvertedCandle& a,
               const ConvertedCandle& b)
            {
                return a.timestampValue ==
                       b.timestampValue;
            }
        );

    candles.erase(
        newEnd,
        candles.end()
    );

    // =========================================================
    // WRITE FINAL DATASET
    // =========================================================

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

    output
        << "timestamp,open,high,low,close,volume\n";

    for (const auto& candle : candles)
    {
        output
            << candle.timestamp << ','
            << std::fixed
            << std::setprecision(8)
            << candle.open << ','
            << candle.high << ','
            << candle.low << ','
            << candle.close << ','
            << candle.volume
            << '\n';
    }

    output.close();

    // =========================================================
    // SUMMARY
    // =========================================================

    std::cout
        << "\n===== DATA CONVERSION RESULT =====\n";

    std::cout
        << "Input files     : "
        << inputFiles.size()
        << '\n';

    std::cout
        << "Output file     : "
        << outputFile
        << '\n';

    std::cout
        << "Converted rows  : "
        << candles.size()
        << '\n';

    std::cout
        << "Skipped rows    : "
        << skippedRows
        << '\n';

    if (candles.empty())
    {
        std::cerr
            << "Error: No valid candles were converted.\n";

        return false;
    }

    std::cout
        << "Conversion successful.\n";

    return true;
}
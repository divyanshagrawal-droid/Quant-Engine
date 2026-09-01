#include "../include/Candle.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<Candle> readCSV(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::vector<Candle> candles;
    std::string line;

    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string value;

        Candle candle;

        std::getline(ss, candle.timestamp, ',');

        std::getline(ss, value, ',');
        candle.open = std::stod(value);

        std::getline(ss, value, ',');
        candle.high = std::stod(value);

        std::getline(ss, value, ',');
        candle.low = std::stod(value);

        std::getline(ss, value, ',');
        candle.close = std::stod(value);

        std::getline(ss, value, ',');
        candle.volume = std::stod(value);

        candles.push_back(candle);
    }

    return candles;
}
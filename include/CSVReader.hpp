#pragma once

#include "Candle.hpp"

#include <string>
#include <vector>

std::vector<Candle> readCSV(
    const std::string& filename
);
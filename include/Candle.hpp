#pragma once

#include <string>

struct Candle {
    std::string timestamp;
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};
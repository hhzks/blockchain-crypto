#pragma once
#include <string>
#include <cstdint>

namespace SHA256{
    std::string hash(const uint8_t* data, size_t length);
    std::string hash(const std::string& input);
}
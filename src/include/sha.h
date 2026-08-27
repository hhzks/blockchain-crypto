#pragma once
#include <array>
#include <string>
#include <cstdint>

namespace SHA256{
    using Digest = std::array<uint8_t, 32>;

    std::string hash(const uint8_t* data, size_t length);
    std::string hash(const std::string& input);

    // The digest as bytes. Hashing a hex rendering doubles the input to every
    // subsequent SHA-256 call, which matters when digests are chained.
    Digest hashRaw(const uint8_t* data, size_t length);
}
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <bit>

namespace SHA256 {
    const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    const uint32_t H0[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    namespace {        
        uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
        
        uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
        
        uint32_t sig0(uint32_t x) { return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22); }
        
        uint32_t sig1(uint32_t x) { return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25); }
        
        uint32_t sigma0(uint32_t x) { return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3); }
        
        uint32_t sigma1(uint32_t x) { return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10); }
        
        uint32_t bytesToWord(const uint8_t* bytes) {
            return (static_cast<uint32_t>(bytes[0]) << 24) |
                   (static_cast<uint32_t>(bytes[1]) << 16) |
                   (static_cast<uint32_t>(bytes[2]) << 8)  |
                   (static_cast<uint32_t>(bytes[3]));
        }
        
        void processBlock(const uint8_t* block, uint32_t* hash) {
            uint32_t w[64];
            uint32_t a, b, c, d, e, f, g, h;
            uint32_t t1, t2;
            
            for (int i = 0; i < 16; i++) {
                w[i] = bytesToWord(&block[i * 4]);
            }
            
            for (int i = 16; i < 64; i++) {
                w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
            }
            
            a = hash[0];
            b = hash[1];
            c = hash[2];
            d = hash[3];
            e = hash[4];
            f = hash[5];
            g = hash[6];
            h = hash[7];
            
            for (int i = 0; i < 64; i++) {
                t1 = h + sig1(e) + ch(e, f, g) + K[i] + w[i];
                t2 = sig0(a) + maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            
            hash[0] += a;
            hash[1] += b;
            hash[2] += c;
            hash[3] += d;
            hash[4] += e;
            hash[5] += f;
            hash[6] += g;
            hash[7] += h;
        }
    }
    
    std::string hash(const uint8_t* data, size_t length) {
        uint32_t hash[8];
        for (int i = 0; i < 8; i++) {
            hash[i] = H0[i];
        }
        
        size_t totalBits = length * 8;
        size_t paddingBits = (448 - (totalBits % 512) + 512) % 512;
        size_t paddingBytes = paddingBits / 8;
        size_t totalLength = length + paddingBytes + 8;
        
        std::vector<uint8_t> paddedMessage(totalLength);
        std::memcpy(paddedMessage.data(), data, length);
        paddedMessage[length] = 0x80;
        
        for (int i = 0; i < 8; i++) {
            paddedMessage[totalLength - 8 + i] = static_cast<uint8_t>(totalBits >> (56 - i * 8));
        }
        
        for (size_t i = 0; i < totalLength; i += 64) {
            processBlock(&paddedMessage[i], hash);
        }
        
        std::stringstream ss;
        for (int i = 0; i < 8; i++) {
            ss << std::hex << std::setw(8) << std::setfill('0') << hash[i];
        }
        
        return ss.str();
    }

    std::string hash(const std::string& input) {
        return hash(reinterpret_cast<const uint8_t*>(input.c_str()), input.length());
    }
    
}
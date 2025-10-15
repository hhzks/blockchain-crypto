#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <compare>

class BigInt {
private:
    std::vector<uint32_t> digits;
    bool negative;
    void normalise();                 
    
public:
    // Constructors
    BigInt();
    BigInt(std::vector<uint32_t> value);
    BigInt(long long value);
    BigInt(const BigInt& other);


    std::string toHex() const;
    std::string toBinary() const;
    
    BigInt& operator=(const BigInt& other) = default;
    BigInt& operator=(long long value);
    BigInt& operator=(std::vector<uint32_t> value);

    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator-() const;
    BigInt operator*(const BigInt& other) const;
    BigInt square() const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;

    BigInt& operator+=(const BigInt& other);
    BigInt& operator-=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    BigInt& operator/=(const BigInt& other);
    BigInt& operator%=(const BigInt& other);

    BigInt inverse(const BigInt& m) const;
    

    [[nodiscard]] bool operator==(const BigInt& other) const;
    [[nodiscard]] std::strong_ordering operator<=>(const BigInt& other) const;
    [[nodiscard]] std::strong_ordering operator<=>(const long long other) const;
};
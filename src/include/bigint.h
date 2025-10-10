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
    
    // Assignment operators
    BigInt& operator=(const BigInt& other) = default;
    BigInt& operator=(long long value);
    BigInt& operator=(std::vector<uint32_t> value);
    
    // Arithmetic operators
    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator-() const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;
    

    [[nodiscard]] bool operator==(const BigInt& other) const;
    [[nodiscard]] std::strong_ordering operator<=>(const BigInt& other) const;
};
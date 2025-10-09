#include <vector>
#include <string>
#include <sstream>

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
    
    // Assignment operators
    BigInt& operator=(const BigInt& other);
    BigInt& operator=(long long value);
    
    // Arithmetic operators
    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator-() const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;
    
    std::strong_ordering operator<=>(const BigInt& other) const;
};
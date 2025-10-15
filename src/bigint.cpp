#include "include/bigint.h"
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <compare>

BigInt::BigInt(){
    negative = false;
}

BigInt::BigInt(std::vector<uint32_t> value){
    /*
    * Constructor using a C++ vector representing a large integer
    * Vector should be in ascending order starting from the least significant 32-bit integer
    */
    negative = false;
    digits = value.empty() ? std::vector<uint32_t> {0} : value;
    normalise();
}

BigInt::BigInt(long long value){
    digits.clear();
    if (value == 0){
        digits.push_back(0);
        negative = false;
        return;
    }
    uint64_t abs_value = value < 0 ? -value : value;
    negative = value < 0 ? true : false; 
    while(abs_value){
        digits.push_back(static_cast<uint32_t>(abs_value & UINT32_MAX));
        abs_value >>= 32;
    }
}

BigInt::BigInt(const BigInt& other){
    negative = other.negative;
    digits.clear();
    for(size_t i = 0; i < other.digits.size(); i++){
        digits.push_back(other.digits[i]);
    }
}

BigInt& BigInt::operator=(long long value){
    *this = BigInt(value);
    return *this;
}

BigInt& BigInt::operator=(std::vector<uint32_t> value){
    *this = BigInt(value);
    return *this;
}



void BigInt::normalise(){
    while(!digits.empty() && digits.back() == 0){digits.pop_back();}
}

std::string BigInt::toHex() const{
    if (digits.empty()) {std::cout << "burh"; throw std::runtime_error("Cannot represent an unitialized integer");}
    std::stringstream hex_stringstream;
    if (negative){
        hex_stringstream << "-";
    }
    hex_stringstream << "0x";
    for(size_t i = digits.size(); i > 0; --i){
        hex_stringstream << std::hex << std::setw(8) << std::setfill('0') << digits[i-1];
    }
    return hex_stringstream.str();
}

std::string BigInt::toBinary() const{
    if (digits.empty()) {throw std::runtime_error("Cannot represent an unitialized integer");}
    std::stringstream binary_stringstream;
    if (negative){
        //binary_stringstream << "-";
    }
    for (size_t i = digits.size(); i > 0; i--){
        binary_stringstream << std::hex << std::setw(32) << std::setfill('0') << digits[i-1];
    }
    return binary_stringstream.str();
}

BigInt BigInt::operator+(const BigInt& other) const {
    if (negative && !other.negative) {
        return -*this - other ;
    }
    if (!negative && other.negative){
        return *this - -other;
    }
    
    BigInt result;
    uint64_t sum = 0;
    size_t max_size = std::max(digits.size(), other.digits.size());
    
    for (size_t i = 0; i < max_size || sum; ++i) {
        if (i < digits.size()) {sum += digits[i];}
        if (i < other.digits.size()) {sum += other.digits[i];}
        
        result.digits.push_back(static_cast<uint32_t>(sum & UINT32_MAX));
        sum >>= 32;
    }
    result.normalise();
    result.negative = negative ? true : false;
    return result;
}

BigInt BigInt::operator-(const BigInt& other) const{
    if (negative && !other.negative){
        return -(-*this + other);
    }
    if (!negative && other.negative){
        return *this + -other;
    }

    BigInt result;
    bool previous_value_was_negative = false;
    size_t max_size = std::max(digits.size(), other.digits.size());
    const BigInt* bigger_number;
    const BigInt* smaller_number;
    if (*this >= other){
        bigger_number = this;
        smaller_number = &other;
        result.negative = false;
    }
    else{
        bigger_number = &other;
        smaller_number = this;
        result.negative = true;
    }

    for (size_t i = 0; i < max_size; ++i){
        if (i < smaller_number->digits.size()){
            result.digits.push_back(bigger_number->digits[i] - smaller_number->digits[i] - 1 * previous_value_was_negative);
        }
        else{
            result.digits.push_back(bigger_number->digits[i] - 1 * previous_value_was_negative);
        }
        if (i < smaller_number->digits.size() && bigger_number->digits[i] < smaller_number->digits[i]){
            previous_value_was_negative = true;  
        }
        else{
            previous_value_was_negative = false;
        }
    }

    result.normalise();
    result.negative = negative ? !result.negative : result.negative;
    return result;
}

BigInt BigInt::operator-() const{
    BigInt result;
    result.digits = digits;
    return result;
}

BigInt BigInt::operator*(const BigInt& other) const{
    BigInt result;

    if ((digits.size() == 1 && digits[0] == 0) || (other.digits.size() == 1 && other.digits[0] == 0)) {
        result.digits = {0};
        result.negative = false;
        return result;
    }

    result.digits.resize(digits.size() + other.digits.size(), 0);

    for (size_t i = 0; i < digits.size(); ++i) {
        if (digits[i] == 0) continue;
        
        uint64_t carry = 0;
        uint64_t multiplicand = static_cast<uint64_t>(digits[i]);
        
        for (size_t j = 0; j < other.digits.size(); ++j) {
            uint64_t product = multiplicand * other.digits[j] + carry + result.digits[i + j];
            result.digits[i + j] = static_cast<uint32_t>(product & UINT32_MAX);
            carry = product >> 32;
        }
        
        if (carry) {
            result.digits[i + other.digits.size()] = static_cast<uint32_t>(carry);
        }
    }
    
    result.negative = negative != other.negative;

    result.normalise();
    return result;
}

BigInt BigInt::square() const{
    return *this * *this;
}

BigInt BigInt::operator/(const BigInt& other) const{
    if (other.digits.size() == 1 && other.digits[0] == 0) {
        throw std::runtime_error("Division by zero is undefined");
    }

    if (digits.size() == 1 && digits[0] == 0) {
        return BigInt(0);
    }

    bool result_negative = negative != other.negative;

    BigInt dividend = *this;
    BigInt divisor = other;
    dividend.negative = false;
    divisor.negative = false;

    if (dividend < divisor) {
        return BigInt(0);
    }

    BigInt quotient;
    BigInt remainder;

    for (int i = dividend.digits.size() * 32 - 1; i >= 0; --i) {
        remainder = remainder * BigInt(2);
        
        uint32_t word_index = i / 32;
        uint32_t bit_index = i % 32;
        if (word_index < dividend.digits.size() && 
            (dividend.digits[word_index] & (1ULL << bit_index))) {
            remainder += BigInt(1);
        }
        
        if (remainder >= divisor) {
            remainder -= divisor;
            
            if (quotient.digits.size() <= word_index) {
                quotient.digits.resize(word_index + 1, 0);
            }
            quotient.digits[word_index] |= (1ULL << bit_index);
        }
    }

    quotient.negative = result_negative;
    quotient.normalise();
    return quotient;
}

BigInt BigInt::operator%(const BigInt& other) const{
    if (other.digits.size() == 1 && other.digits[0] == 0) {
        throw std::runtime_error("Modulo zero is undefined");
    }

    BigInt abs_this = *this;
    abs_this.negative = false;
    BigInt abs_other = other;
    abs_other.negative = false;
    if (abs_this < abs_other) {
        return *this;
    }

    BigInt quotient = *this / other;
    BigInt remainder = *this - (quotient * other);
    remainder.negative = negative;
    return remainder;
}

BigInt& BigInt::operator+=(const BigInt& other) {
    *this = *this + other;
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& other) {
    *this = *this - other;
    return *this;
}

BigInt& BigInt::operator*=(const BigInt& other) {
    *this = *this * other;
    return *this;
}

BigInt& BigInt::operator/=(const BigInt& other) {
    *this = *this / other;
    return *this;
}

BigInt& BigInt::operator%=(const BigInt& other) {
    *this = *this % other;
    return *this;
}

BigInt BigInt::inverse(const BigInt& mod) const{
    BigInt current_mod = mod;
    BigInt divisor = *this;
    divisor = ((divisor % mod) + mod) % mod;

    BigInt m0 = current_mod;
    BigInt x0 = BigInt(0);
    BigInt x1 = BigInt(1);

    if (current_mod == BigInt(1))
        return BigInt(0);

    while (divisor > BigInt(1)) {
        BigInt q = divisor / current_mod;
        BigInt t = current_mod;

        current_mod = divisor % current_mod;
        divisor = t;
        t = x0;

        x0 = x1 - q * x0;
        x1 = t;
    }

    if (x1 < BigInt(0))
        x1 += m0;

    std::cout << "inverse of " << (*this).toHex() << " is " << ((x1 % mod + mod) % mod).toHex() << '\n';
    return (x1 % mod + mod) % mod;
}

bool BigInt::operator==(const BigInt& other) const{
    if ((!negative && other.negative) || (negative && !other.negative)) {return false;}
    if (digits.size() != other.digits.size()) {return false;}
    for (size_t i = 0; i < digits.size(); i++){
        if (digits[i] != other.digits[i]) {return false;} 
    }
    return true;
}

std::strong_ordering BigInt::operator<=>(const BigInt& other) const{
    if (!negative && other.negative) {return std::strong_ordering::greater;}
    if (negative && !other.negative) {return std::strong_ordering::less;}
    if (digits.size() > other.digits.size()) {return std::strong_ordering::greater;}
    if (digits.size() < other.digits.size()) {return std::strong_ordering::less;}

    for (size_t i = digits.size(); i > 0; --i){
        if (digits[i-1] > other.digits[i-1]){
            if (negative) {return std::strong_ordering::less;} 
            else {return std::strong_ordering::greater;}
        } 
        if (digits[i-1] < other.digits[i-1]){
            if (negative) {return std::strong_ordering::greater;} 
            else {return std::strong_ordering::less;}
        }
    }
    return std::strong_ordering::equal;
}

std::strong_ordering BigInt::operator<=>(const long long other) const{
    if (!negative && other < 0) {return std::strong_ordering::greater;}
    if (negative && other >= 0) {return std::strong_ordering::less;}
    if (digits.size() == 1){
        if (digits[0] < other) {return std::strong_ordering::less;}
        else if (digits[0] == other) {return std::strong_ordering::equal;}
    }
    return std::strong_ordering::greater; //if digits.size >= 2 then the bigint is at least 2^32, so greater than other
}

/*
int main(){
    std::vector<uint32_t> vec_1 = {12,214,1235};
    std::vector<uint32_t> vec_2 = {125, 12343, 31};
    BigInt bigint_1 = BigInt(vec_1);
    BigInt bigint_2 = BigInt(vec_2);
    BigInt bigint_3 = BigInt(125);
    std::cout << bigint_1.toHex() << std::endl;
    std::cout << bigint_2.toHex() << std::endl;
    std::cout << bigint_3.toHex() << std::endl;
    std::cout << (bigint_1+bigint_2).toHex() << std::endl;
    std::cout << (bigint_2+bigint_3).toHex() << std::endl;
    std::cout << (bigint_1*bigint_2).toHex() << " mult" << std::endl;
    const BigInt P ({0xFFFFFC2F, 0xFFFFFFFE,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF});
    const BigInt A (0);
    const BigInt B (7);
    const BigInt GX ({0x16F81798, 0x59F2815B, 0x2DCE28D9, 0x029BFCDB, 0xCE870B07, 0x55A06295, 0xF9DCBBAC, 0x79BE667E});
    const BigInt GY ({0xFB10D4B8, 0x9C47D08F, 0xA6855419, 0xFD17B448, 0x0E1108A8, 0x5DA4FBFC, 0x26A3C465, 0x483ADA77});
    std:: cout << P.toHex() << std::endl << A.toHex() << std::endl << B.toHex() << std::endl << GX.toHex() << std::endl << GY.toHex() << std::endl;
    return 0;
    
}*/

int main(){
    std::vector<uint32_t> vec_1 = {12,2142,4};
    std::vector<uint32_t> vec_2 = {125};
    BigInt bigint_1 = BigInt(vec_1);
    BigInt bigint_2 = BigInt(vec_2);
    std::cout << bigint_1.toHex() << std::endl;
    std::cout << bigint_2.toHex() << std::endl;
    std::cout << (bigint_1/bigint_2).toHex() << std::endl;
    std::cout << (bigint_1%bigint_2).toHex() << std::endl;
}
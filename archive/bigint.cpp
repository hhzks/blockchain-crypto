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
    * Vector should be in little endian starting from the least significant 32-bit integer
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

//hex string constructor (0x......)
BigInt::BigInt(std::string value){
negative = false;
digits.clear();

if (value.empty()) {
    digits.push_back(0);
    return;
}

size_t start_pos = 2;
if (value[0] == '-') {
    negative = true;
    start_pos = 3;
}

while (start_pos < value.size() && value[start_pos] == '0') {
    start_pos++;
}

if (start_pos >= value.size()) {
    digits.push_back(0);
    negative = false;
    return;
}

for (size_t i = value.size(); i > start_pos; ) {
    size_t chunk_start = (i > start_pos + 8) ? i - 8 : start_pos;
    std::string chunk = value.substr(chunk_start, i - chunk_start);
    
    uint32_t digit_value = 0;
    for (char c : chunk) {
        digit_value <<= 4;
        if (c >= '0' && c <= '9') {
            digit_value |= (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit_value |= (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit_value |= (c - 'A' + 10);
        } else {
            throw std::invalid_argument("Invalid hexadecimal character");
        }
    }
    
    digits.push_back(digit_value);
    i = chunk_start;
}

normalise();
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
    if (digits.empty()) {throw std::runtime_error("Cannot represent an unitialized integer");}
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
        binary_stringstream << "-";
    }
    for (size_t i = digits.size(); i > 0; i--){
        for (int bit = 31; bit >= 0; --bit) {
            binary_stringstream << ((digits[i-1] >> bit) & 1);
        }
    }
    return binary_stringstream.str();
}

BigInt BigInt::abs() const{
    BigInt result = *this;
    result.negative = false;
    return result;
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
    result.negative = !(negative);
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

BigInt BigInt::squared() const{
    return *this * *this;
}

std::pair<BigInt, BigInt> BigInt::divmod(const BigInt& other) const{
    if (other.digits.size() == 1 && other.digits[0] == 0) {
        throw std::runtime_error("Division by zero is undefined");
    }

    if (digits.size() == 1 && digits[0] == 0) {
        return std::make_pair(BigInt(0),BigInt(0));
    }

    BigInt dividend = *this;
    BigInt divisor = other;
    dividend.negative = false;
    divisor.negative = false;

    if (dividend < divisor) {
        return std::make_pair(BigInt(0), dividend);
    }

    BigInt quotient;
    BigInt remainder;

    size_t total_bits = dividend.digits.size() * 32;
    size_t quotient_words = (total_bits + 31) / 32;
    quotient.digits.resize(quotient_words, 0);

    for (int i = total_bits - 1; i >= 0; --i) {
        remainder = remainder * 2;
        
        uint32_t word_index = i / 32;
        uint32_t bit_index = i % 32;
        if (word_index < dividend.digits.size() && 
            (dividend.digits[word_index] & (1ULL << bit_index))) {
            remainder += 1;
        }
        
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient.digits[word_index] |= (1ULL << bit_index);
        }
    }

    quotient.negative = negative != other.negative;
    if (quotient.negative && remainder > 0){
        quotient -= 1;
        remainder = other - remainder;
    }
    quotient.normalise();
    remainder.normalise();
    return std::make_pair(quotient, remainder);
}

BigInt BigInt::operator/(const BigInt& other) const{
    return divmod(other).first;
    }


BigInt BigInt::operator%(const BigInt& other) const{
    return divmod(other).second;
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
    divisor %= mod;

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

    x1.normalise();
    if (x1 < BigInt(0))
        x1 += m0;

    return x1 % mod;
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
    
} 

int main(){
    std::vector<uint32_t> vec_1 = {471};
    std::vector<uint32_t> vec_2 = {34};
    BigInt bigint_1 = BigInt(vec_1);
    BigInt bigint_2 = BigInt(vec_2);
    std::cout << bigint_1.toHex() << std::endl;
    std::cout << bigint_2.toHex() << std::endl;
    std::cout << (bigint_1/bigint_2).toHex() << std::endl;
    std::cout << ((-bigint_1)/bigint_2).toHex() << std::endl;
    std::cout << (bigint_1%bigint_2).toHex() << std::endl;
    std::cout << ((-bigint_1)%bigint_2).toHex() << std::endl;
    std::cout << (bigint_2.inverse(bigint_1).toHex()) << std::endl;
}
*/
#include "include/bigint.h"

BigInt::BigInt(std::vector<uint32_t> value){
    digits = value.empty() ? std::vector<uint32_t> {0} : value;
}

BigInt::BigInt(long long value){
    digits.clear();
    if (value == 0){
        digits.push_back(0);
        return;
    }
    uint64_t abs_value = value < 0 ? -value : value;
    negative = value < 0 ? true : false; 
    while(abs_value){
        digits.push_back(static_cast<uint32_t>(abs_value & UINT32_MAX));
        abs_value >>= 32;
    }
}

BigInt::BigInt(long long value){
    BigInt::BigInt(static_cast<uint64_t>(value));
}

BigInt::BigInt(const BigInt& other){
    negative = other.negative;
    digits = other.digits;
}

void BigInt::normalise(){
    while(!digits.empty() && digits.back() == 0){digits.pop_back();}
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
    BigInt bigger_number;
    BigInt smaller_number;
    if (*this >= other){
        bigger_number = *this;
        smaller_number = other;
        result.negative = false;
    }
    else{
        bigger_number = other;
        smaller_number = *this;
        result.negative = true;
    }

    for (size_t i = max_size-1; i < max_size; ++i){
        if (i < smaller_number.digits.size()){
            result.digits.push_back(bigger_number.digits[i]-smaller_number.digits[i]-1*previous_value_was_negative);
        }
        else{
            result.digits.push_back(bigger_number.digits[i]-1*previous_value_was_negative);
        }
        if (i < smaller_number.digits.size() && bigger_number.digits[i] < smaller_number.digits[i]){
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
    result.negative = !negative;
    return result;
}

std::strong_ordering BigInt::operator<=>(const BigInt& other) const{
    if (!negative && other.negative) {return std::strong_ordering::greater;}
    if (negative && !other.negative) {return std::strong_ordering::greater;}
    if (digits.size() > other.digits.size()) {return std::strong_ordering::greater;}
    if (digits.size() < other.digits.size()) {return std::strong_ordering::less;}

    for (size_t i = digits.size() - 1; i >= 0; --i){
        if (digits[i] > other.digits[i]){
            if (negative && other.negative) {return std::strong_ordering::less;} 
            else {return std::strong_ordering::greater;}
        } 
        if (digits[i] < other.digits[i]){
            if (negative && other.negative) {return std::strong_ordering::greater;} 
            else {return std::strong_ordering::less;}
        }
    }
    return std::strong_ordering::equal;
}

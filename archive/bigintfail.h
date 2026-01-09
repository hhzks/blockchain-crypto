/*
    BigInt Library for C++

    MIT License
    
    Copyright (c) 2024 Samuel Herts
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#ifndef BigInt_H_
#define BigInt_H_

#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <algorithm>
#include <random>
#include <iomanip>

namespace BigInt {

    class BigInt {
    public:
        //                   LLONG_MAX = 9'223'372'036'854'775'807
        static constexpr auto MAX_SIZE = 1'000'000'000'000'000'000LL;

        BigInt() {is_neg = false; vec.emplace_back(0);}

        BigInt(const std::string &s)
        {
            if (!is_BigInt(s)) {throw std::runtime_error("Invalid Big Integer.");}
            if(s[0] == '-')
            {
                *this = BigInt(s.substr(1));
                if (*this == 0)
                    throw std::runtime_error("Invalid Big Integer.");
                is_neg = true;
            }
            else
            {
                vec = string_to_vector(s);
            }
        }

        BigInt(const char c)
        {
            int temp = static_cast<unsigned char>(c);
            if (isdigit(temp)) {
                *this = BigInt(char_to_int(c));
            } else {
                throw std::runtime_error("Invalid Big Integer has been fed.");
            }
        }

        BigInt(const unsigned int n) : BigInt(static_cast<long long>(n)) {}

        BigInt(const int n) : BigInt(static_cast<long long>(n)) {}

        BigInt(const long n) : BigInt(static_cast<long long>(n)) {}

        BigInt(const double n) : BigInt(static_cast<long long>(n)) {}

        BigInt(const long long n) {
            if (n == 0)
            {
                is_neg = false;
                vec.push_back(0);
                return;
            }
            is_neg = (n<0);

            unsigned long long val = n;
            if (is_neg)
            {
                val = 0ULL - val;
            }
            vec.reserve(2);

            while (val > 0) {
                vec.emplace_back(val % MAX_SIZE);
                val /= MAX_SIZE;
            }
            std::reverse(vec.begin(), vec.end());
        }

        BigInt(unsigned long long n) {

          if ( n >= MAX_SIZE )
          {
            vec.emplace_back(n / MAX_SIZE);
            vec.emplace_back(n % MAX_SIZE);
          }
          else{
            vec.emplace_back(n);
          }
        }

        BigInt(const BigInt &n) { *this = n; }

        BigInt(const char* n) : BigInt(std::string(n)) {}

        BigInt(std::vector<long long> n) {this->vec = std::move(n);}

        BigInt& operator=(const BigInt& other)
        {
            if (this == &other)
                return *this;
            this->is_neg = other.is_neg;
            this->vec = other.vec;

            return *this;
        }

        explicit operator int() const
        {
            return static_cast<int>(vec.back());
        }

        explicit operator long long() const
        {
          return vec.back();
        }

        explicit operator std::string() const
        {
              return (this->is_neg ? "-" : "") + vector_to_string(this->vec);
        }

        friend std::ostream &operator<<(std::ostream &stream, const BigInt &n)
        {
            stream << std::string(n);
            return stream;
        }

        BigInt operator+=(const BigInt &rhs)
        {
            if (*this == 0 && rhs == 0) return *this;
            if (*this == 0) {*this = rhs; return *this;}
            if (rhs != 0) {
                *this = add(*this, rhs);
            }
            return *this;
        }

        BigInt operator+(const BigInt &rhs) const
        {
            BigInt result = *this;
            result += rhs;
            return result;
        }

        BigInt operator-=(const BigInt &rhs)
        {
            *this = subtract(*this, rhs);
            return *this;
        }

        BigInt operator-(const BigInt &rhs) const
        {
            BigInt result = *this;
            result -= rhs;
            return result;
        }

        BigInt operator*=(const BigInt &rhs)
        {
            *this = multiply(*this, rhs);
            return *this;
        }

        BigInt operator*(const BigInt &rhs) const
        {
            BigInt result = *this;
            result *= rhs;
            return result;
        }

        BigInt &operator/=(const BigInt &rhs)
        {
            *this = divide(*this, rhs);
            return *this;
        }

        BigInt operator/(const BigInt &rhs) const
        {
            BigInt result = *this;
            result /= rhs;
            return result;
        }

        BigInt operator%=(const BigInt &rhs)
        {
                std::cout << "ae" << std::endl;

            *this = mod(*this, rhs);
            return *this;
        }

        BigInt operator%(const BigInt &rhs) const
        {
                std::cout << "na" << std::endl;

            BigInt result = *this;
            result %= rhs;
            return result;
        }

        BigInt operator++()
        {
            *this += 1;
            return *this;
        }

        BigInt operator++(int)
        {
            BigInt tmp(*this);
            operator++();
            return tmp;
        }

        BigInt operator--()
        {
            *this -= 1;
            return *this;
        }

        BigInt operator--(int)
        {
            BigInt tmp(*this);
            operator--();
            return tmp;
        }

        BigInt operator-() const &
        {
            BigInt temp = *this;
            if (temp == 0) return temp;
            temp.is_neg = !this->is_neg;
            return temp;
        }

        BigInt operator-() &&
        {
            if (*this == 0) return *this;
            this->is_neg = !this->is_neg;
            return *this;
        }

        friend bool operator==(const BigInt &l, const BigInt &r)
        {
            if (l.is_neg != r.is_neg)
            {
                return false;
            }
            return l.vec == r.vec;
        }

        friend bool operator!=(const BigInt &l, const BigInt &r)
        { return !(l == r); }

        friend bool operator<(const BigInt &lhs, const BigInt &rhs)
        {
            return less_than(lhs, rhs);
        }

        friend bool operator>(const BigInt &l, const BigInt &r)
        { return r < l; }

        friend bool operator<=(const BigInt &l, const BigInt &r)
        { return r >= l; }

        friend bool operator>=(const BigInt &l, const BigInt &r)
        { return !(l < r); }

        explicit operator bool() const {
            return !(vec.size() == 1 && vec.front() == 0);
        }

        friend std::hash<BigInt>;

        static BigInt pow(const BigInt &base, const BigInt &exponent)
        {
            if (exponent == 0) return 1;
            if (exponent == 1) return base;

            const BigInt tmp = pow(base, exponent / 2);
            if (exponent % 2 == 0) {return tmp * tmp;}
            return base * tmp * tmp;
        }

        static BigInt squared(const BigInt &base)
        {
            return pow(base, 2);
        }

        static BigInt maximum(const BigInt &lhs, const BigInt &rhs)
        {
            return lhs > rhs ? lhs : rhs;
        }

        static BigInt minimum(const BigInt &lhs, const BigInt &rhs)
        {
            return lhs > rhs ? rhs : lhs;
        }

        static BigInt abs(const BigInt &s)
        {
            if (!is_negative(s)) return s;

            BigInt temp = s;
            temp.is_neg = false;

            return temp;
        }

        static BigInt abs(BigInt&& s)
        {
            s.is_neg = false;
            return s;
        }

        static BigInt sqrt(const BigInt &);

        static BigInt log2(const BigInt &);

        static BigInt log10(const BigInt &);

        static BigInt logwithbase(const BigInt &, const BigInt &);

        static BigInt antilog2(const BigInt &);

        static BigInt antilog10(const BigInt &);

        static void swap(BigInt &, BigInt &);

        static BigInt gcd(const BigInt &, const BigInt &);

        static BigInt inverse(const BigInt &, const BigInt &);

        static BigInt lcm(const BigInt &lhs, const BigInt &rhs)
        {
            return (lhs * rhs) / gcd(lhs, rhs);
        }

        static BigInt factorial(const BigInt &);

        static bool is_even(const BigInt &input)
        {
            return !(input.vec.back() & 1);
        }

        static bool is_negative(const BigInt &input)
        {
            return input.is_neg;
        }

        static bool is_prime(const BigInt &);

        static BigInt sum_of_digits(const BigInt& input)
        {
            BigInt sum;
            for (auto base : input.vec) {
                for (sum = 0; base > 0; sum += base % 10, base /= 10);
            }
            return sum;
        }

        /**
         * @brief Converts a BigInt to its binary (base 2) representation as a string.
         *
         * This method converts the BigInt to a binary string representation.
         * Negative numbers are prefixed with a minus sign.
         *
         * @param input The BigInt to convert to binary.
         * @return A std::string representing the binary representation of the BigInt.
         */
        static std::string toBinary(const BigInt& input)
        {
            if (input == 0) return "0";
            
            std::string result;
            BigInt temp = abs(input);
            
            while (temp > 0) {
                result = (temp % 2 == 0 ? '0' : '1') + result;
                temp /= 2;
            }
            
            if (is_negative(input)) {
                result = "-" + result;
            }
            
            return result;
        }

        /**
         * @brief Generates a random positive BigInt of a specified length.
         *
         * This method ensures the resulting BigInt is valid by using a random device
         * and engine for non-deterministic seeding.
         *
         * @param length The number of digits the generated BigInt should have.
         * @return A BigInt object representing the randomly generated number.
         */
        static BigInt random(size_t length);

    private:
        std::vector<long long> vec{};
        bool is_neg{false};

        // Function Definitions for Internal Uses

        static BigInt trim(BigInt input) {
            while (input.vec.size() > 1 && input.vec.front() == 0) {
                input.vec.erase(input.vec.begin());
            }
            if (input.vec.empty()) {
                input.vec.push_back(0);
                input.is_neg = false;
            }
            return input;
        }

        static std::vector<long long> string_to_vector(std::string input);

        static std::string vector_to_string(const std::vector<long long>& input);

        static BigInt add(const BigInt &, const BigInt &);

        static BigInt subtract(const BigInt &, const BigInt &);

        static BigInt multiply(const BigInt &, const BigInt &);

        static BigInt divide(const BigInt &, const BigInt &);

        static BigInt mod(const BigInt &lhs, const BigInt &rhs)
        {
            std::cout << lhs << " " << rhs << std::endl;
            if (rhs == 0) {throw std::domain_error("Attempted to modulo by zero.");}
            if (lhs < rhs) {
                return lhs;
            }
            if (lhs == rhs) {
                return 0;
            }

            if (rhs == 2)
            {
                return !is_even(lhs);
            }

            return lhs - ((lhs / rhs) * rhs);
        }

        static bool is_BigInt(const std::string &);

        static int count_digits(const BigInt&);

        static int char_to_int(const char input)
        {
            return input - '0';
        }

        static int int_to_char(const int input)
        {
            return input + '0';
        }

        static BigInt negate(const BigInt& input)
        {
            BigInt temp = input;
            temp.is_neg = !temp.is_neg;
            return temp;
        }

        static BigInt negate(BigInt&& input)
        {
            input.is_neg = !input.is_neg;
            return input;
        }

        static bool less_than(const BigInt& lhs, const BigInt& rhs)
        {
            if (is_negative(lhs) && is_negative(rhs))
            {
                return less_than(abs(rhs), abs(lhs));
            }

            if (is_negative(lhs) || is_negative(rhs))
            {
                return is_negative(lhs);
            }

            if(lhs.vec.size() == rhs.vec.size())
            {
                return lhs.vec < rhs.vec;
            }

            return lhs.vec.size() < rhs.vec.size();
        }
    };


    inline bool BigInt::is_BigInt(const std::string &s)
    {
        if (s.empty() || (s.length() > 1 && s[0] == '0'))
            return false;

        if (s[0] == '-') {
            return s.find_first_not_of("0123456789", 1) == std::string::npos;
        }
        return s.find_first_not_of("0123456789", 0) == std::string::npos;
    }

    inline BigInt BigInt::add(const BigInt &lhs, const BigInt &rhs)
    {
        bool negate_answer = false;
        // Ensure both are positive
        if (is_negative(lhs) && is_negative(rhs)) negate_answer = true;
        else if (is_negative(lhs)) return subtract(rhs, abs(lhs));
        else if (is_negative(rhs)) return subtract(lhs, abs(rhs));

        // Ensure LHS is larger than RHS
        if (lhs.vec.size() < rhs.vec.size()) return add(rhs, lhs);

        // Prepare result vector with enough space (max size + 1 for potential carry)
        std::vector<long long> result;
        result.reserve(lhs.vec.size() + 1);

        long long carry = 0;
        auto it_l = lhs.vec.rbegin();
        auto it_r = rhs.vec.rbegin();

        while (it_l != lhs.vec.rend()) {
            long long sum = *it_l + carry;
            if (it_r != rhs.vec.rend()) {
                sum += *it_r;
                ++it_r;
            }

            if (sum >= MAX_SIZE) {
                sum -= MAX_SIZE;
                carry = 1;
            } else {
                carry = 0;
            }

            result.push_back(sum);
            ++it_l;
        }

        if (carry > 0) {
            result.push_back(carry);
        }

        std::reverse(result.begin(), result.end());
        BigInt result_BigInt {std::move(result)};
        return negate_answer ? negate(result_BigInt) : result_BigInt;
    }

    inline std::pair<int, long long> subtract_with_borrow(const long long lhs, const long long rhs)
    {
        if (lhs < rhs)
        {
            // Borrow needs to happen
            auto result = (lhs + BigInt::MAX_SIZE) - rhs;
            return {1, result}; // 1 represents a borrow
        }

        auto result = lhs - rhs;
        return {0, result}; // 0 means no borrow
    }

    inline BigInt BigInt::subtract(const BigInt &lhs, const BigInt &rhs)
    {
        // Ensure LHS is larger than RHS, and both are positive
        if (rhs == 0) return lhs;
        if (lhs == rhs) return 0;

        if (is_negative(lhs) && is_negative(rhs))
        {
            return subtract(abs(rhs), abs(lhs));
        }
        if (is_negative(rhs))
        {
            return add(lhs, abs(rhs));
        }
        if (is_negative(lhs))
        {
            return add(lhs, negate(rhs));
        }
        if (lhs < rhs)
        {
            return negate(subtract(rhs, lhs));
        }

        BigInt full_rhs =  rhs;
        std::vector<std::pair<int, long long>> borrow_result(lhs.vec.size());
        // Fill the smaller to match the larger size
        while (lhs.vec.size() > full_rhs.vec.size())
        {
            full_rhs.vec.insert(full_rhs.vec.begin(), 0);
        }

        std::transform(lhs.vec.rbegin(), lhs.vec.rend(), full_rhs.vec.rbegin(), borrow_result.rbegin(),
                       subtract_with_borrow);

        std::vector<long long> final(lhs.vec.size());
        for (int i = borrow_result.size() - 1; i >= 0; --i) {
            final[i] += borrow_result[i].second;
            if (borrow_result[i].first)
                final[i - 1] -= borrow_result[i].first;
        }

        return trim(final);
    }

    inline BigInt BigInt::multiply(const BigInt &lhs, const BigInt &rhs)
    {
        if (lhs == 0 || rhs == 0) return 0;
        if (lhs == 1) return rhs;
        if (rhs == 1) return lhs;

        if (is_negative(lhs) && is_negative(rhs))
        {
            return (abs(lhs) * abs(rhs));
        }
        if (is_negative(lhs) || is_negative(rhs))
        {
            return negate(abs(lhs) * abs(rhs));
        }
        if (lhs < rhs)
        {
            return multiply(rhs, lhs);
        }

        std::vector<long long> result(lhs.vec.size() + rhs.vec.size(), 0);

        for (auto it_lhs = lhs.vec.rbegin(); it_lhs != lhs.vec.rend(); ++it_lhs)
        {
            for (auto it_rhs = rhs.vec.rbegin(); it_rhs != rhs.vec.rend(); ++it_rhs)
            {
                // Calculate the product and the corresponding indices in the result vector
                // use 128 bits to carefully store overflow
                __int128 mul = static_cast<__int128>(*it_lhs) * static_cast<__int128>(*it_rhs);
                auto pos_low_it = result.rbegin() + (std::distance(lhs.vec.rbegin(), it_lhs) + std::distance(rhs.vec.rbegin(), it_rhs));
                auto pos_high_it = pos_low_it + 1;

                // Add the product to the result vector
                *pos_low_it += mul % MAX_SIZE;
                if (pos_high_it != result.rend())
                {
                    *pos_high_it += mul / MAX_SIZE;
                }

                // Handle carry
                if (*pos_low_it >= MAX_SIZE) {
                    if (pos_high_it != result.rend()) {
                        *pos_high_it += *pos_low_it / MAX_SIZE;
                    }
                    *pos_low_it %= MAX_SIZE;
                }
            }
        }

        // Handle carries for remaining positions
        for (auto r_iter = result.rbegin(); r_iter != result.rend() - 1; ++r_iter) {
            if (*r_iter >= MAX_SIZE)
            {
                *(r_iter + 1) += *r_iter / MAX_SIZE;
                *r_iter %= MAX_SIZE;
            }
        }

        return trim(result);
    }


    inline BigInt BigInt::divide(const BigInt &numerator, const BigInt &denominator)
    {
        if (denominator == 0)
        {
            throw std::domain_error("Attempted to divide by zero.");
        }
        if (numerator == denominator) return 1;
        if (denominator == 1) return numerator;
        if (numerator == 0) return 0;

        if (is_negative(numerator) && is_negative(denominator))
        {
            return divide(abs(numerator), abs(denominator));
        }
        if (is_negative(numerator) || is_negative(denominator))
        {
            return negate(divide(abs(numerator), abs(denominator)));
        }
        if (numerator < denominator)
        {
            return 0;
        }
        if (numerator.vec.size() <= 1)
        {
            return numerator.vec.back() / denominator.vec.back();
        }

        BigInt remainder = numerator;
        BigInt quotient = 0;

        auto count = count_digits(remainder) - count_digits(denominator) - 1;

        auto numerator_size = pow(10, count);

        auto temp = denominator * numerator_size;

        while (denominator * numerator_size < remainder)
        {
            temp = denominator * numerator_size;
            remainder -= temp;
            quotient += numerator_size;
            count = count_digits(remainder) - count_digits(denominator) - 1;


            if (numerator_size <= 1)
            {
                quotient += remainder / denominator;
                break;
            }
            if (remainder.vec.size() <= 1)
            {
                quotient += remainder.vec.back() / denominator.vec.back();
                break;
            }
            numerator_size = pow(10, count);
        }

        return quotient;
    }

    inline BigInt BigInt::sqrt(const BigInt &input)
    {
        if (is_negative(input))
            throw std::domain_error("Square root of a negative number is complex");

        if (input == 0 || input == 1)
            return input;

        BigInt oom = log10(input / 2);
        oom /= 2;
        BigInt low_end = pow(10, (oom));
        BigInt high_end = pow(10, (oom) + 2);
        BigInt mid_point, square, answer;
        while (low_end <= high_end) {
            mid_point = (low_end + high_end) / 2;
            square = mid_point * mid_point;
            if (square == input)
                return mid_point;

            if (square < input) {
                low_end = mid_point + 1;
                answer = mid_point;
            } else {
                high_end = mid_point - 1;
            }
        }
        return answer;
    }


    inline BigInt BigInt::log2(const BigInt &input)
    {
        if (is_negative(input) || input == 0)
            throw std::domain_error("Invalid input for natural log");

        if (input == 1)
            return 0;

        if (input.vec.size() == 1) {
            return std::log2(input.vec.back());
        }

        BigInt exponent = 0;
        while (pow(2, exponent) <= input) {
            ++exponent;
        }

        return exponent;
        // TODO: Convert to using division after checking bigO of division vs multiplication
//    std::string logVal = "-1";
//    while(s != "0") {
//        logVal = add(logVal, "1");
//        s = divide(s, "2");
//    }
//    return logVal;
    }

    inline BigInt BigInt::log10(const BigInt &input)
    {
        if (is_negative(input) || input == 0)
            throw std::domain_error("Invalid input for log base 10");

        if (input == 1)
            return 0;

        if (input.vec.size() == 1) {
            return std::log10(input.vec.back());
        }
        int count = 0;
        for (auto number : input.vec) {
            count += count_digits(number);
        }

        return count - 1;
    }

    inline BigInt BigInt::logwithbase(const BigInt &input, const BigInt &base)
    {
        auto top = log2(input);
        auto bottom = log2(base);
        auto answer = divide(top, bottom);
        return answer;
    }

    inline BigInt BigInt::antilog2(const BigInt &input)
    {
        return pow(2, input);
    }

    inline BigInt BigInt::antilog10(const BigInt &input)
    {
        return pow(10, input);
    }

    inline void BigInt::swap(BigInt &lhs, BigInt &rhs)
    {
        const BigInt temp = lhs;
        lhs = rhs;
        rhs = temp;
    }

    inline BigInt BigInt::gcd(const BigInt &lhs, const BigInt &rhs)
    {
        BigInt temp_l = lhs, temp_r = rhs, remainder;
        if (rhs > lhs)
            swap(temp_l, temp_r);

        while (rhs > 0) {
            remainder = temp_l % temp_r;
            temp_l = temp_r;
            temp_r = remainder;
        }
        return temp_l;
    }

    inline BigInt BigInt::inverse(const BigInt &a, const BigInt &mod)
    {
        BigInt temp_mod = mod;
        BigInt temp_a = a;

        if (is_negative(temp_a))
        {
            temp_a = temp_a % mod;
        }

        BigInt y_prev = 0;
        BigInt y = 1;

        BigInt q;
        BigInt y_before;
        BigInt temp_a_before;
        while (temp_a > 1){
            std::cout << "EIOTJI)OAT" << std::endl;
                std::cout << temp_a << " " << temp_mod << std::endl;

            std::cout << "hehehehe" << std::endl;
            q = temp_mod / a;

            y_before = y;
            y = y_prev - q * y;
            y_prev = y_before;

            temp_a_before = temp_a;
                std::cout << temp_mod << " " << temp_a << std::endl;

            temp_a = temp_mod % temp_a;
                std::cout << "a" << std::endl;

            temp_mod = temp_a_before;
        }
        std::cout << "done" << std::endl;
        return y % mod;

    }

    inline BigInt BigInt::factorial(const BigInt &input)
    {
        if (is_negative(input)) {
            throw std::runtime_error("Factorial of Negative Integer is not defined.");
        }
        if (input == 0)
            return 1;

        BigInt ans = 1;
        BigInt temp = input;
        while (temp != 0) {
            ans *= temp;
            temp -= 1;
        }
        return ans;
    }

    inline bool BigInt::is_prime(const BigInt &s)
    {
        if (is_negative(s) || s == 1)
            return false;

        if (s == 2 || s == 3 || s == 5)
            return true;

        if (is_even(s) || s % 5 == 0)
            return false;


        BigInt i;
        for (i = 3; i * i <= s; i += 2) {
            if ((s % i) == 0) {
                return false;
            }
        }
        return true;
    }

    inline BigInt BigInt::random(const size_t length) {
        constexpr char charset[] = "0123456789";
        std::default_random_engine rng(std::random_device{}());

        // Distribution for the first digit (1-9)
        std::uniform_int_distribution<> first_dist(1, 9);
        // Distribution for the other digits (0-9)
        std::uniform_int_distribution<> dist(0, 9);

        // Lambda to generate the first character
        auto randchar_first = [charset, &first_dist, &rng]() { return charset[first_dist(rng)]; };
        // Lambda to generate subsequent characters
        auto randchar = [charset, &dist, &rng]() { return charset[dist(rng)]; };

        // Create a string with the specified length
        std::string str(length, 0);

        // Generate the first character separately to ensure it's not '0'
        str[0] = randchar_first();

        // Generate the remaining characters
        std::generate_n(str.begin() + 1, length - 1, randchar);

        return {str};
    }
    

    inline std::vector<long long> BigInt::string_to_vector(std::string input) {
        // Break into chunks of 18 characters
        std::vector<long long> result;
        constexpr int chunk_size = 18;
        const int size = input.size();

        if (size > chunk_size)
        {
            // Pad the length to get appropriate sized chunks
            input.insert(0, chunk_size - (size % chunk_size), '0');
        }
        for (int i = 0; i < input.size(); i+=chunk_size)
        {
            std::string temp_str = input.substr(i, chunk_size);
            result.emplace_back(stoll(temp_str));
        }

        return result;
    }

    inline std::string BigInt::vector_to_string(const std::vector<long long>& input) {
        std::stringstream ss;
        bool first = true;
        for (auto partial : input) {
            if (first) {
                ss << partial;  // No padding for the first number
                first = false;
            } else {
                ss << std::setw(18) << std::setfill('0') << partial;  // Pad to 18 digits
            }
        }
        return ss.str();
    }



    inline int BigInt::count_digits(const BigInt & input) {
        std::string my_string = vector_to_string(input.vec);
        return static_cast<int>(my_string.length()) - 1;
    }

} // namespace::BigInt

template<>
struct std::hash<BigInt::BigInt> {
    std::size_t operator()(const BigInt::BigInt& input) const
    {
        std::size_t seed = input.vec.size();
        for(auto x : input.vec) {
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

#endif /* BigInt_H_ */
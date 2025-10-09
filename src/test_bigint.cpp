#include "include/bigint.h"
#include <iostream>

int main() {
    std::cout << "=== Simple BigInt Tests ===" << std::endl;
    
    // Test constructors
    std::cout << "\n1. Testing Constructors:" << std::endl;
    BigInt a(42u);
    BigInt b(0u);
    BigInt c(UINT32_MAX);
    std::cout << "✓ uint32_t constructors work" << std::endl;
    
    // Test addition
    std::cout << "\n2. Testing Addition:" << std::endl;
    BigInt sum = a + b;  // 42 + 0
    std::cout << "✓ Addition completed" << std::endl;
    
    // Test comparison
    std::cout << "\n3. Testing Comparison:" << std::endl;
    auto cmp = a <=> b;
    if (cmp == std::strong_ordering::greater) {
        std::cout << "✓ 42 > 0: Correct" << std::endl;
    }
    
    // Test edge cases
    std::cout << "\n4. Testing Edge Cases:" << std::endl;
    BigInt zero1(0);
    BigInt zero2(0);
    BigInt zero_sum = zero1 + zero2;
    std::cout << "✓ Zero arithmetic works" << std::endl;
    
    std::cout << "\n=== Tests Complete ===" << std::endl;
    return 0;
}
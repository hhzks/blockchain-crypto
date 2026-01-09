#include "include/ECCrypto.h"
#include "include/sha.h"
#include "include/bigint.h"
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>

namespace ECCrypto {

BigInt ECPoint::getX() const{
    return x;
}

BigInt ECPoint::getY() const{
    return y;
}

ECPoint ECPoint::pointAtInfinity() const{
    ECPoint result = ECPoint {0, 0, p, a, b};
    result.isInfinity = true;
    return result;
}

ECPoint ECPoint::doubledPoint() const{
    if (y == 0) {
        return pointAtInfinity();
    }

    BigInt slope = (((squared(x) * 3) + a) * inverse(y*2,p)) % p;
    BigInt new_x = (squared(slope) - (x * 2)) % p;
    BigInt new_y = ((slope * (x - new_x) - y) % p + p) % p;
    return ECPoint {new_x, new_y, p, a, b};
}

ECPoint ECPoint::operator+(const ECPoint& other) const{
    if (isInfinity && other.isInfinity){return pointAtInfinity();}
    if (!isInfinity && other.isInfinity) {return *this;}
    if (isInfinity && !other.isInfinity) {return other;}

    if (x == other.x && y == other.y){
        return doubledPoint();
    }

    if (x == other.x && y != other.y) {
        return pointAtInfinity();
    }

    BigInt slope = (inverse((other.y - y) * (other.x - x),p)) % p;
    BigInt new_x = (squared(slope) - x - other.x) % p;
    BigInt new_y = (slope * (x - new_x) - y) % p;
    return ECPoint {new_x, new_y, p, a, b};
}

ECPoint ECPoint::operator*(const BigInt& scalar) const{
    if (scalar == 0) {
        return pointAtInfinity();
    }
    
    ECPoint current = *this;
    std::string binary_str = scalar.to_binary_string();
    std::cout << binary_str << std::endl;
    
    size_t i = 0;
    while (i < binary_str.size() && binary_str[i] != '1') {i++;}
    
    for (++i; i < binary_str.size(); i++){
        current = current.doubledPoint();
        if (binary_str[i] == '1') {
            current += *this;
        }
        std::cout << current.getX() << " " << current.getY() << std::endl;
    }
    
    return current;
}

ECPoint ECPoint::operator+=(const ECPoint& other){
    *this = *this + other;
    return *this;
}

ECPoint ECPoint::operator*=(const BigInt& scalar){
    *this = *this * scalar;
    return *this;
}

/*
std::unique_ptr<KeyPair> generateKeyPair(){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
    
    std::vector<uint32_t> privateKeyData(8);
    BigInt privateKey = 0;

    while (privateKey == 0){
        for (int i = 0; i < 8; i++) {
            privateKeyData[i] = dis(gen);
        }
        
        privateKey = privateKeyData;
        privateKey %= secp256k1::N;
    }
    
    ECPoint publicKey = G * privateKey;
    
    auto keyPair = std::make_unique<KeyPair>();
    keyPair->privateKey = privateKey;
    keyPair->publicKey = publicKey;
    
    return keyPair;

}
*/
}

int main() {
    
    std::cout << inverse(BigInt(44), BigInt(67)) << std::endl;
    std::cout << BigInt(-19) % BigInt(31) << std::endl;
    ECCrypto::ECPoint G {ECCrypto::secp256k1::GX, ECCrypto::secp256k1::GY, ECCrypto::secp256k1::P, ECCrypto::secp256k1::A, ECCrypto::secp256k1::B};
    std::cout << "x = " << G.getX().to_string() << std::endl;
    std::cout << "y = " << G.getY().to_string() << std::endl;
    // Test vector for Point Addition
    // Let's add G to itself and compare with 2*G
    ECCrypto::ECPoint pointAdditionResult = G + G;
    // Output the results of Point Addition
    std::cout << "Point Addition Result:" << std::endl;
    std::cout << "x = " << pointAdditionResult.getX().to_string() << std::endl;
    std::cout << "y = " << pointAdditionResult.getY().to_string() << std::endl;
    ECCrypto::ECPoint G1 {ECCrypto::secp256k1::GX, ECCrypto::secp256k1::GY, ECCrypto::secp256k1::P, ECCrypto::secp256k1::A, ECCrypto::secp256k1::B};
    ECCrypto::ECPoint expectedAdditionResult = G1 * BigInt(2);
    std::cout << "Scalar Multiplication Result:" << std::endl;
    std::cout << "x = " << expectedAdditionResult.getX().to_string() << std::endl;
    std::cout << "y = " << expectedAdditionResult.getY().to_string() << std::endl;
    // Comparing and output the test result for Point Addition
    bool isAdditionCorrect = pointAdditionResult.getX() == expectedAdditionResult.getX() && pointAdditionResult.getY() == expectedAdditionResult.getY();
    std::cout << "Point Addition Test " << (isAdditionCorrect ? "PASSED" : "FAILED") << std::endl;
    // Test vector for Scalar Multiplication
    // Scalar value k = 3
    // G.print();
    ECCrypto::ECPoint scalarMultiplicationResult = G * BigInt(3);
    // Expected values for 3*G (you can compute this using an external ECC calculator or library)
    std::string expected_x_str = "112711660439710606056748659173929673102114977341539408544630613555209775888121";
    std::string expected_y_str = "25583027980570883691656905877401976406448868254816295069919888960541586679410";
    // Output of the results of Scalar Multiplication
    std::cout << "Scalar Multiplication Result:" << std::endl;
    std::cout << "x = " << scalarMultiplicationResult.getX().to_string() << std::endl;
    std::cout << "y = " << scalarMultiplicationResult.getY().to_string() << std::endl;
    // Comparing and output the test result for Scalar Multiplication
    bool isMultiplicationCorrect = (scalarMultiplicationResult.getX().to_string() == expected_x_str) &&
                                   (scalarMultiplicationResult.getY().to_string() == expected_y_str);
    std::cout << "Scalar Multiplication Test " << (isMultiplicationCorrect ? "PASSED" : "FAILED") << std::endl;
    return 0;
}
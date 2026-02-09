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
    BigInt new_y = (slope * (x - new_x) - y) % p;
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

    BigInt slope = ((other.y - y) * inverse(other.x - x,p)) % p;
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
    
    size_t i = 0;
    while (i < binary_str.size() && binary_str[i] != '1') {i++;}
    
    for (++i; i < binary_str.size(); i++){
        current = current.doubledPoint();
        if (binary_str[i] == '1') {
            current += *this;
        }

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

std::unique_ptr<ECCrypto::KeyPair> generateKeyPair(){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;
        
    BigInt private_key = 0;
    std::string priv_str;
        
    // Generate a random 256-bit private key
    for (int i = 0; i < 4; i++) {
        unsigned long long rand_val = dis(gen);
        std::stringstream ss;
        ss << std::hex << rand_val;
        priv_str += ss.str();
    }
        
    private_key = BigInt(priv_str, 16);
    private_key = private_key % ECCrypto::secp256k1::N;
        
    if (private_key == 0) {
        private_key = 1;
    }
        
    return keyPairFromPrivateKey(private_key);
}

std::unique_ptr<KeyPair> keyPairFromPrivateKey(const BigInt& private_key){
    return std::make_unique<KeyPair>(private_key, G * (private_key % ECCrypto::secp256k1::N));
}

std::unique_ptr<KeyPair> keyPairFromPrivateKeyHex(const std::string& private_key_hex){
    BigInt private_key {BigInt(private_key_hex,16)};
    return std::make_unique<KeyPair>(private_key, G * (private_key_hex % ECCrypto::secp256k1::N));
}

Signature signHash(const Hash& hash, const BigInt& privateKey){
    return Signature();
}

int main() {
    
    std::cout << BigInt(-19) % BigInt(31) << std::endl;
    ECCrypto::ECPoint point = ECCrypto::G;
    std::cout << "x = " << point.getX().to_string() << std::endl;
    std::cout << "y = " << point.getY().to_string() << std::endl;
    point *= BigInt("29618965053773872102369003108549999020865001328514371697420486241125147475161");
    std::cout << "x = " << point.getX().to_string() << std::endl;
    std::cout << "y = " << point.getY().to_string() << std::endl;
    return 0;
}

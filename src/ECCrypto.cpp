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

namespace secp256k1 {
    //curve parameters
    const BigInt P ({0xFFFFFC2F, 0xFFFFFFFE,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF});
    const BigInt A (0);
    const BigInt B (7);

    //generator point stuff
    const BigInt GX ({0x16F81798, 0x59F2815B, 0x2DCE28D9, 0x029BFCDB, 0xCE870B07, 0x55A06295, 0xF9DCBBAC, 0x79BE667E});
    const BigInt GY ({0xFB10D4B8, 0x9C47D08F, 0xA6855419, 0xFD17B448, 0x0E1108A8, 0x5DA4FBFC, 0x26A3C465, 0x483ADA77});
    const BigInt N ({0xD0364141, 0xBFD25E8C, 0xAF48A03B, 0xBAAEDCE6, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF});

};

BigInt ECPoint::getX() const{
    return x;
}

BigInt ECPoint::getY() const{
    return y;
}

ECPoint ECPoint::doubledPoint() const{
    //insert infinity check

    BigInt gradient = ((x.square() * 3) + a) * (y * 2).inverse(p);
    BigInt doubled_x = (gradient.square() - (x * 2)) % p;
    BigInt doubled_y = (gradient * (x - doubled_x) - y) % p;
    return ECPoint {doubled_x, doubled_y, p, a, b};
}

ECPoint ECPoint::operator+(const ECPoint& other) const{
    if (x == other.x && y == other.y){
        return doubledPoint();
    }

    //insert infinity check

    BigInt slope = (y - other.y) * (x - other.x).inverse(p);
    BigInt new_x = (slope.square() - x - other.x) % p;
    BigInt new_y = ((slope * (x - new_x)) - y) % p;
    return ECPoint {new_x, new_y, p, a, b};
}

ECPoint ECPoint::operator*(const BigInt& scalar) const{
    ECPoint current = *this;
    std::string binary_str = scalar.toBinary();
    binary_str = binary_str.substr(3, binary_str.size()-4);
    
    for (size_t i = 0; i < binary_str.size(); i++){
        std::cout << "Index " << i << "\n";
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
    
}

using namespace ECCrypto; 
int main(){
    ECPoint point_1 = ECPoint{secp256k1::GX, secp256k1::GY, secp256k1::P, secp256k1::A, secp256k1::B};
    BigInt multiplier = BigInt(45374698053069);
    std::cout << "Calculating..." << '\n';
    ECPoint result = point_1 * multiplier;
    std::cout << result.getX().toHex() << ' ' << result.getY().toHex() << '\n';
    return 0;
}

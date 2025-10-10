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
    const BigInt b (7);

    //generator point stuff
    const BigInt GX ({0x16F81798, 0x59F2815B, 0x2DCE28D9, 0x029BFCDB, 0xCE870B07, 0x55A06295, 0xF9DCBBAC, 0x79BE667E});
    const BigInt GY ({0xFB10D4B8, 0x9C47D08F, 0xA6855419, 0xFD17B448, 0x0E1108A8, 0x5DA4FBFC, 0x26A3C465, 0x483ADA77});
    const BigInt n ({0xD0364141, 0xBFD25E8C, 0xAF48A03B, 0xBAAEDCE6, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF});

    };


ECPoint ECPoint::operator*(const BigInt& scalar) const{
    
}
    
}
cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_COMPILER "C:/dev/w64devkit/bin/g++.exe")
set(CMAKE_C_COMPILER "C:/dev/w64devkit/bin/gcc.exe")
project(Blockchain)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mining is unusably slow in an unoptimised build, so default to Release
# rather than to CMake's empty (no-optimisation) build type. Multi-config
# generators pick their configuration at build time and are left alone.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CORE_SOURCES
    src/Blockchain.cpp
    src/Block.cpp
    src/Transaction.cpp
    src/utils.cpp
    src/sha.cpp
    src/keystore.cpp
    src/ECCrypto.cpp
    src/Wallet.cpp
    src/P2PNode.cpp
)

set(CORE_HEADERS
    src/include/Blockchain.h
    src/include/Block.h
    src/include/Transaction.h
    src/include/utils.h
    src/include/sha.h
    src/include/keystore.h
    src/include/ECCrypto.h
    src/include/Wallet.h
    src/include/bigint.h
    src/include/P2PMessage.h
    src/include/P2PNode.h
)

add_library(blockchain_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(blockchain_core PUBLIC src src/include)

if(WIN32)
    # bcrypt supplies BCryptGenRandom, the OS entropy source for key material.
    target_link_libraries(blockchain_core PUBLIC ws2_32 bcrypt)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 13+ std::print requires linking libstdc++exp.
    target_link_libraries(blockchain_core PUBLIC stdc++exp)
endif()

# GCC/Clang only: bigint.h is built on __int128, which MSVC does not provide.
target_compile_options(blockchain_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(blockchain src/main.cpp)
target_link_libraries(blockchain PRIVATE blockchain_core)

option(BUILD_TESTS "Build the test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Core sources: ${CORE_SOURCES}")
message(STATUS "Core headers: ${CORE_HEADERS}")

cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_COMPILER "C:/dev/w64devkit/bin/g++.exe")
set(CMAKE_C_COMPILER "C:/dev/w64devkit/bin/gcc.exe")
project(Blockchain)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/Blockchain.cpp
    src/Block.cpp
    src/Transaction.cpp
    src/utils.cpp
    src/sha.cpp
    src/ECCrypto.cpp
    src/Wallet.cpp
    src/P2PNode.cpp
)

set(HEADERS
    src/include/Blockchain.h
    src/include/Block.h
    src/include/Transaction.h
    src/include/utils.h
    src/include/sha.h
    src/include/ECCrypto.h
    src/include/Wallet.h
    src/include/bigint.h
    src/include/P2PMessage.h
    src/include/P2PNode.h
)

add_executable(blockchain ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(blockchain ws2_32)
endif()

if(MSVC)
    target_compile_options(blockchain PRIVATE /W4)
else()
    target_compile_options(blockchain PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "Source files: ${SOURCES}")
message(STATUS "Header files: ${HEADERS}")


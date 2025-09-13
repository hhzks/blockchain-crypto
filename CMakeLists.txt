cmake_minimum_required(VERSION 3.16)
project(Blockchain)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


set(SOURCES
    src/main.cpp
    src/Blockchain.cpp
    src/Block.cpp
    src/Transaction.cpp
    src/utils.cpp
    src/sha.cpp
)

set(HEADERS
    src/include/Blockchain.h
    src/include/Block.h
    src/include/Transaction.h
    src/include/utils.h
    src/include/sha.h
)

add_executable(blockchain ${SOURCES} ${HEADERS})

# Compiler flags
if(MSVC)
    target_compile_options(blockchain PRIVATE /W4)
else()
    target_compile_options(blockchain PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Print debug information
message(STATUS "Source files: ${SOURCES}")
message(STATUS "Header files: ${HEADERS}")


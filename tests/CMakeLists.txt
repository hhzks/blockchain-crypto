include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

set(TEST_SOURCES
    unit/utils_test.cpp
    unit/sha_test.cpp
    unit/bigint_test.cpp
    unit/bigint_v2_test.cpp
    unit/eccrypto_test.cpp
    unit/transaction_test.cpp
    unit/block_test.cpp
    unit/blockchain_test.cpp
    unit/wallet_test.cpp
    unit/p2p_message_test.cpp
    integration/chain_lifecycle_test.cpp
    integration/p2p_loopback_test.cpp
)

add_executable(blockchain_tests ${TEST_SOURCES})
target_link_libraries(blockchain_tests PRIVATE blockchain_core Catch2::Catch2WithMain)
target_include_directories(blockchain_tests PRIVATE support)

if(NOT MSVC)
    target_compile_options(blockchain_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

catch_discover_tests(blockchain_tests)

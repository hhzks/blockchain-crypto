cmake_minimum_required(VERSION 3.16)
project(Blockchain VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)


# Set vcpkg triplet
set(VCPKG_TARGET_TRIPLET x64-windows)
# Check if we're using vcpkg and set up paths manually if needed
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows")
    message(STATUS "Found vcpkg installation, setting OpenSSL paths manually")
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib")
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.lib")
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    set(OPENSSL_FOUND TRUE)
    
    # Create imported targets
    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto STATIC IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    endif()
    
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL STATIC IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "OpenSSL::Crypto")
    endif()
else()
    # Fallback to system OpenSSL
    find_package(OpenSSL REQUIRED)
endif()

# Include directories
include_directories(include)

# Explicitly list all source files
set(SOURCES
    src/main.cpp
    src/Blockchain.cpp
    src/Block.cpp
    src/Transaction.cpp
    src/utils.cpp
)

# List header files for IDE support
set(HEADERS
    include/Blockchain.h
    include/Block.h
    include/Transaction.h
    include/utils.h
)

# Create executable with all source files
add_executable(blockchain ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(blockchain OpenSSL::SSL OpenSSL::Crypto)

# Add Windows-specific libraries that OpenSSL depends on
if(WIN32)
    target_link_libraries(blockchain ws2_32 crypt32)
endif()

# Compiler flags
if(MSVC)
    target_compile_options(blockchain PRIVATE /W4)
else()
    target_compile_options(blockchain PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Print debug information
message(STATUS "Source files: ${SOURCES}")
message(STATUS "Header files: ${HEADERS}")

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
    # Find all OpenSSL DLLs
    file(GLOB OPENSSL_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin/libcrypto*.dll")
    file(GLOB OPENSSL_SSL_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin/libssl*.dll")
    
    # Copy DLLs to output directory after build
    foreach(DLL ${OPENSSL_DLLS} ${OPENSSL_SSL_DLLS})
        add_custom_command(TARGET blockchain POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}"
            $<TARGET_FILE_DIR:blockchain>
            COMMENT "Copying ${DLL}")
    endforeach()
    
    message(STATUS "Will copy OpenSSL DLLs to output directory")
endif()
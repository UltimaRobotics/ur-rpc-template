cmake_minimum_required(VERSION 3.16)
project(UrRpcCppWrapper VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
    ../../ur-rpc-template
    ../../ur-rpc-template/deps/cJSON
    ../../ur-rpc-template/deps/ur-logger-api
    ../../ur-rpc-template/deps/mqtt-client
)

# Add the ur-rpc-template subdirectory
add_subdirectory(../../ur-rpc-template ur-rpc-template)

# Create C++ client executables
add_executable(cpp_basic_client cpp_basic_client.cpp)
target_link_libraries(cpp_basic_client 
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

add_executable(cpp_bidirectional_client cpp_bidirectional_client.cpp)
target_link_libraries(cpp_bidirectional_client 
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

add_executable(cpp_ssl_client cpp_ssl_client.cpp)
target_link_libraries(cpp_ssl_client 
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

add_executable(cpp_relay_client cpp_relay_client.cpp)
target_link_libraries(cpp_relay_client 
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

add_executable(cpp_advanced_features cpp_advanced_features.cpp)
target_link_libraries(cpp_advanced_features 
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

# Copy configuration files to build directory
configure_file(cpp_basic_config.json ${CMAKE_BINARY_DIR}/cpp_basic_config.json COPYONLY)
configure_file(cpp_bidirectional_config.json ${CMAKE_BINARY_DIR}/cpp_bidirectional_config.json COPYONLY)
configure_file(cpp_ssl_config.json ${CMAKE_BINARY_DIR}/cpp_ssl_config.json COPYONLY)
configure_file(cpp_relay_config.json ${CMAKE_BINARY_DIR}/cpp_relay_config.json COPYONLY)
configure_file(cpp_advanced_config.json ${CMAKE_BINARY_DIR}/cpp_advanced_config.json COPYONLY)
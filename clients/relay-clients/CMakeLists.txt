cmake_minimum_required(VERSION 3.12)
project(relay_clients)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find PkgConfig for mosquitto
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Include directories
include_directories(../../ur-rpc-template)
include_directories(../../ur-rpc-template/deps/cJSON)
include_directories(../../ur-rpc-template/deps/ur-logger-api)

# Add subdirectory for ur-rpc-template
add_subdirectory(../../ur-rpc-template ur-rpc-template)

# Basic Relay Client
add_executable(basic_relay_client
    basic_relay_client.c
)

target_link_libraries(basic_relay_client
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    m
)

target_compile_options(basic_relay_client PRIVATE ${MOSQUITTO_CFLAGS_OTHER})

# SSL Relay Client
add_executable(ssl_relay_client
    ssl_relay_client.c
)

target_link_libraries(ssl_relay_client
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    m
)

target_compile_options(ssl_relay_client PRIVATE ${MOSQUITTO_CFLAGS_OTHER})

# Conditional Relay Client
add_executable(conditional_relay_client
    conditional_relay_client.c
)

target_link_libraries(conditional_relay_client
    ur-rpc-template
    ${MOSQUITTO_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    m
)

target_compile_options(conditional_relay_client PRIVATE ${MOSQUITTO_CFLAGS_OTHER})

# Copy configuration files to build directory
configure_file(basic_relay_config.json ${CMAKE_CURRENT_BINARY_DIR}/basic_relay_config.json COPYONLY)
configure_file(ssl_relay_config.json ${CMAKE_CURRENT_BINARY_DIR}/ssl_relay_config.json COPYONLY)
configure_file(conditional_relay_config.json ${CMAKE_CURRENT_BINARY_DIR}/conditional_relay_config.json COPYONLY)
cmake_minimum_required(VERSION 3.10)
project(enhanced_ssl_clients C)

# Set C99 standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Use pkg-config to find OpenSSL
pkg_check_modules(SSL REQUIRED openssl)

# Add ur-rpc-template as subdirectory
add_subdirectory(../../ur-rpc-template ur-rpc-template)

# Include directories for headers
include_directories(${CMAKE_CURRENT_BINARY_DIR}/ur-rpc-template/deps/cJSON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/cJSON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/ur-logger-api)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/mqtt-client/src)

# Build enhanced client executables with SSL support
add_executable(enhanced_client_a enhanced_client_a.c)
add_executable(enhanced_client_b enhanced_client_b.c)

# Set dependencies to ensure all libraries are built first
add_dependencies(enhanced_client_a cJSON mqtt-client-static ur-rpc-template)
add_dependencies(enhanced_client_b cJSON mqtt-client-static ur-rpc-template)

# Link libraries
target_link_libraries(enhanced_client_a 
    ur-rpc-template
    cJSON
    mqtt-client-static
    ${SSL_LIBRARIES}
    Threads::Threads
    m
)

target_link_libraries(enhanced_client_b 
    ur-rpc-template
    cJSON
    mqtt-client-static
    ${SSL_LIBRARIES}
    Threads::Threads
    m
)

# Include SSL directories
target_include_directories(enhanced_client_a PRIVATE ${SSL_INCLUDE_DIRS})
target_include_directories(enhanced_client_b PRIVATE ${SSL_INCLUDE_DIRS})

# Copy configuration files to build directory
configure_file(enhanced_client_a_config.json enhanced_client_a_config.json COPYONLY)
configure_file(enhanced_client_b_config.json enhanced_client_b_config.json COPYONLY)
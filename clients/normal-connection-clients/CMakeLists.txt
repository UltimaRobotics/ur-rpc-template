cmake_minimum_required(VERSION 3.10)
project(ur-rpc-clients VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Add the ur-rpc-template library as a subdirectory
add_subdirectory(../../ur-rpc-template ur-rpc-template)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/cJSON
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/mqtt-client/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur-rpc-template/deps/ur-logger-api
)

# Client A executable
add_executable(client_a client_a.c)

# Client B executable  
add_executable(client_b client_b.c)

# Link libraries to Client A
target_link_libraries(client_a 
    PRIVATE ur-rpc-template
    PRIVATE cJSON
    PRIVATE mqtt-client-static
    PRIVATE ur-logger-api-static
    PRIVATE Threads::Threads
    PRIVATE m
)

# Link libraries to Client B
target_link_libraries(client_b 
    PRIVATE ur-rpc-template
    PRIVATE cJSON
    PRIVATE mqtt-client-static
    PRIVATE ur-logger-api-static
    PRIVATE Threads::Threads
    PRIVATE m
)

# Set static linking preference
set_target_properties(client_a PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

set_target_properties(client_b PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

# Copy configuration files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/client_a_config.json 
               ${CMAKE_CURRENT_BINARY_DIR}/client_a_config.json COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/client_b_config.json 
               ${CMAKE_CURRENT_BINARY_DIR}/client_b_config.json COPYONLY)

# Install targets
install(TARGETS client_a client_b
    RUNTIME DESTINATION bin
)

install(FILES 
    client_a_config.json 
    client_b_config.json 
    DESTINATION etc/ur-rpc-clients
)
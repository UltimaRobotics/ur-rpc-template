cmake_minimum_required(VERSION 3.10)
project(ur-rpc-template VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# TODO: Add mosquitto and UUID libraries when available

# Find pthread
find_package(Threads REQUIRED)

# Add local cJSON directory
add_subdirectory(deps/cJSON)

# Add local mqtt-client directory
add_subdirectory(deps/mqtt-client)

# Add local nlohmann/json directory
add_subdirectory(deps/nlohmann)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Library sources
set(LIB_SOURCES
    ur-rpc-template.c
)

# Library headers  
set(LIB_HEADERS
    ur-rpc-template.h
)

# Create the library
add_library(ur-rpc-template STATIC ${LIB_SOURCES} ${LIB_HEADERS})

# Include directories
target_include_directories(ur-rpc-template 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/cJSON
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/mqtt-client/src
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/nlohmann/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../ur-logger-api
)

# Link libraries
target_link_libraries(ur-rpc-template PRIVATE cJSON)
target_link_libraries(ur-rpc-template PRIVATE mqtt-client-static)
target_link_libraries(ur-rpc-template PRIVATE ur-logger-api-static)
target_link_libraries(ur-rpc-template PRIVATE Threads::Threads)
target_link_libraries(ur-rpc-template PRIVATE m)

# Install targets
install(TARGETS ur-rpc-template
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIB_HEADERS} DESTINATION include)

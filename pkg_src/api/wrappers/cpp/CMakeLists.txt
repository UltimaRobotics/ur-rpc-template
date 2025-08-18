cmake_minimum_required(VERSION 3.16)
project(UrRpcCppWrapper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type for wrapper: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler for wrapper: ${CMAKE_CXX_COMPILER}")

add_library(ur-rpc-cpp INTERFACE)

target_include_directories(ur-rpc-cpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ur-rpc-cpp INTERFACE
    ur-rpc-template
)

target_compile_features(ur-rpc-cpp INTERFACE cxx_std_17)


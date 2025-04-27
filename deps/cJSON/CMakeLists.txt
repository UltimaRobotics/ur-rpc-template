cmake_minimum_required(VERSION 3.10)
project(cJSONStaticLib)

# Set the output directory for the compiled static library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add the library
add_library(cJSON STATIC cJSON.c cJSON_Utils.c)

# Include the header files
target_include_directories(cJSON PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set properties for the static library
set_target_properties(cJSON PROPERTIES OUTPUT_NAME "cjson")

# Enable position-independent code (PIC) for better compatibility
set_target_properties(cJSON PROPERTIES POSITION_INDEPENDENT_CODE ON)


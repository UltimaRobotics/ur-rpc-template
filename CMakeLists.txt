cmake_minimum_required(VERSION 3.10)
project(ur-rpc-template)

# Set C standard if needed
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add the deps directory as a subdirectory to build mqtt-client-static first
add_subdirectory(deps/mqtt-client)
# add_subdirectory(deps/cJSON)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/mqtt-client/src
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/cJSON
)

# Link against mqtt-client-static
target_link_libraries(ur-rpc-template PRIVATE mqtt-client-static)
target_link_libraries(ur-rpc-template PRIVATE cJSON)

# Install targets if needed (optional)
install(TARGETS ur-rpc-template
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIB_HEADERS} DESTINATION include)

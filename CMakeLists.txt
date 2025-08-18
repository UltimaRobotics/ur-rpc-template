cmake_minimum_required(VERSION 3.10)
project(ur-rpc-template VERSION 1.0.0 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(ENABLE_TESTS "Enable building of tests" OFF)
option(ENABLE_LOGGER "Enable UR Logger API dependency" ON)
option(ENABLE_MQTT "Enable MQTT client dependency" ON)
option(ENABLE_JSON "Enable cJSON dependency" ON)


set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the build type (None Debug Release RelWithDebInfo MinSizeRel)" FORCE)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-variable -Wno-unused-result -Wno-sign-compare")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

if(ENABLE_JSON)
    add_subdirectory(deps/cJSON)
endif()

if(ENABLE_MQTT)
    add_subdirectory(deps/mqtt-client)
endif()

if(ENABLE_LOGGER)
    add_subdirectory(deps/ur-logger-api)
endif()

set(LIB_SOURCES ur-rpc-template.c )

set(LIB_HEADERS ur-rpc-template.h )


add_library(ur-rpc-template ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(ur-rpc-template 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/cJSON
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/mqtt-client/src
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/nlohmann/include
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/ur-logger-api
)

# Conditional linking
if(ENABLE_JSON)
    target_link_libraries(ur-rpc-template PRIVATE cJSON)
endif()

if(ENABLE_MQTT)
    target_link_libraries(ur-rpc-template PRIVATE mqtt-client-static)
endif()

if(ENABLE_LOGGER)
    target_link_libraries(ur-rpc-template PRIVATE ur-logger-api-static)
endif()

target_link_libraries(ur-rpc-template PRIVATE Threads::Threads m)

add_subdirectory(pkg_src/api/wrappers/cpp)

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


install(TARGETS ur-rpc-template
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIB_HEADERS} DESTINATION include)


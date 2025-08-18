cmake_minimum_required(VERSION 3.10)

project(ur-logger-api 
    VERSION 1.0.0
    DESCRIPTION "Universal Router Logger API Library"
    LANGUAGES C)

option(BUILD_SHARED "Build shared ur-logger-api library" OFF)
option(BUILD_STATIC "Build static ur-logger-api library" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

find_package(Threads REQUIRED)

set(LOGGER_SOURCES logger.c)
set(LOGGER_HEADERS logger.h)

# Object library (shared sources)
add_library(logger_objects OBJECT ${LOGGER_SOURCES})
target_include_directories(logger_objects PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/attached_assets>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(logger_objects PUBLIC Threads::Threads)

if(BUILD_STATIC)
    add_library(ur-logger-api-static STATIC $<TARGET_OBJECTS:logger_objects>)
    set_target_properties(ur-logger-api-static PROPERTIES
        OUTPUT_NAME ur-logger-api
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_include_directories(ur-logger-api-static PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/attached_assets>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ur-logger-api-static PUBLIC Threads::Threads)
    add_library(ur-logger-api::static ALIAS ur-logger-api-static)
endif()

if(BUILD_SHARED)
    add_library(ur-logger-api-shared SHARED $<TARGET_OBJECTS:logger_objects>)
    set_target_properties(ur-logger-api-shared PROPERTIES
        OUTPUT_NAME ur-logger-api
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_include_directories(ur-logger-api-shared PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/attached_assets>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(ur-logger-api-shared PUBLIC Threads::Threads)
    add_library(ur-logger-api::shared ALIAS ur-logger-api-shared)
endif()

include(GNUInstallDirs)




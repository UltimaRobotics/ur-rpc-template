cmake_minimum_required(VERSION 3.10)
project(mqtt-client VERSION 1.0.0 LANGUAGES C)

option(BUILD_SHARED "Build shared mqtt-client library" OFF)
option(BUILD_STATIC "Build static mqtt-client library" ON)

# Always build with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenSSL REQUIRED)

# Enable TLS + threading
add_definitions(-DWITH_TLS -DWITH_THREADING)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.h")

if(BUILD_SHARED)
    add_library(mqtt-client-shared SHARED ${SOURCES} ${HEADERS})

    target_link_libraries(mqtt-client-shared PRIVATE ${OPENSSL_LIBRARIES})
    target_include_directories(mqtt-client-shared
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    set_target_properties(mqtt-client-shared PROPERTIES
        OUTPUT_NAME "mqtt-client"
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${HEADERS}"
    )

    install(TARGETS mqtt-client-shared
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/mqtt-client
    )
endif()

if(BUILD_STATIC)
    add_library(mqtt-client-static STATIC ${SOURCES} ${HEADERS})

    target_link_libraries(mqtt-client-static PRIVATE ${OPENSSL_LIBRARIES})
    target_include_directories(mqtt-client-static
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    set_target_properties(mqtt-client-static PROPERTIES
        OUTPUT_NAME "mqtt-client"
        PUBLIC_HEADER "${HEADERS}"
    )

    install(TARGETS mqtt-client-static
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/mqtt-client
    )
endif()


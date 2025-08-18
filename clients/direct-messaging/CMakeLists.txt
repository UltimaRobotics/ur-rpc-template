cmake_minimum_required(VERSION 3.16)
project(DirectMessagingClients VERSION 1.0.0 LANGUAGES C CXX)

# Set C standard (C11 for stdatomic.h support)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set CXX standard for atomic operations
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Check for OpenSSL (required for TLS support)
pkg_check_modules(OPENSSL REQUIRED openssl)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
endif()

# Include ur-rpc-template project
add_subdirectory(../../ur-rpc-template ${CMAKE_BINARY_DIR}/ur-rpc-template)

# Set the runtime output directory for all targets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Include directories
include_directories(../../ur-rpc-template)
include_directories(../../ur-rpc-template/deps)
include_directories(../../ur-rpc-template/deps/cJSON)
include_directories(../../ur-rpc-template/deps/mqtt-client/include)
include_directories(../../ur-rpc-template/deps/ur-logger-api)

# Standard Request-Response Clients
add_executable(client_requester client_requester.c)
target_link_libraries(client_requester 
    ur-rpc-template 
    mqtt-client-static 
    cJSON
    ur-logger-api-static
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    m
)
target_compile_definitions(client_requester PRIVATE ${OPENSSL_CFLAGS_OTHER})
target_include_directories(client_requester PRIVATE ${OPENSSL_INCLUDE_DIRS})

add_executable(client_responder client_responder.c)
target_link_libraries(client_responder 
    ur-rpc-template 
    mqtt-client-static 
    cJSON
    ur-logger-api-static
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    m
)
target_compile_definitions(client_responder PRIVATE ${OPENSSL_CFLAGS_OTHER})
target_include_directories(client_responder PRIVATE ${OPENSSL_INCLUDE_DIRS})

# Queued Messaging Clients
add_executable(queued_client_1 queued_client_1.c)
target_link_libraries(queued_client_1 
    ur-rpc-template 
    mqtt-client-static 
    cJSON
    ur-logger-api-static
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    m
)
target_compile_definitions(queued_client_1 PRIVATE ${OPENSSL_CFLAGS_OTHER})
target_include_directories(queued_client_1 PRIVATE ${OPENSSL_INCLUDE_DIRS})

add_executable(queued_client_2 queued_client_2.c)
target_link_libraries(queued_client_2 
    ur-rpc-template 
    mqtt-client-static 
    cJSON
    ur-logger-api-static
    ${OPENSSL_LIBRARIES}
    Threads::Threads
    m
)
target_compile_definitions(queued_client_2 PRIVATE ${OPENSSL_CFLAGS_OTHER})
target_include_directories(queued_client_2 PRIVATE ${OPENSSL_INCLUDE_DIRS})

# Copy configuration files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/requester_config.json ${CMAKE_BINARY_DIR}/requester_config.json COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/responder_config.json ${CMAKE_BINARY_DIR}/responder_config.json COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/queued_client_1_config.json ${CMAKE_BINARY_DIR}/queued_client_1_config.json COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/queued_client_2_config.json ${CMAKE_BINARY_DIR}/queued_client_2_config.json COPYONLY)
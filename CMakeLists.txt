cmake_minimum_required(VERSION 3.16)

# Extract version from root README.md ("**Version: major.minor.patch**")
set(_ROOT_README_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ROOT_README_FILE}")
file(READ "${_ROOT_README_FILE}" _ROOT_README_CONTENT)
string(REGEX MATCH "\\*\\*Version:[ \t]*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\*\\*" _VER_MATCH "${_ROOT_README_CONTENT}")
if(NOT _VER_MATCH)
    message(FATAL_ERROR "Could not extract **Version: x.y.z** from ${_ROOT_README_FILE}")
endif()
set(PROJ_VER "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

project(octibot VERSION ${PROJ_VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -UNDEBUG)
endif()

option(OCTIBOT_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(OCTIBOT_BUILD_TESTS "Build octibot CppUTest qualification suite" ON)

if(OCTIBOT_SANITIZE AND NOT MSVC AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer
                        -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

set(OCTI_CORE_SANITIZE ${OCTIBOT_SANITIZE} CACHE BOOL "" FORCE)
set(OCTI_CORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(core)

set(OCTIBOT_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
if(NOT EXISTS "${OCTIBOT_THIRD_PARTY_DIR}/json/include/nlohmann/json.hpp" OR
   NOT EXISTS "${OCTIBOT_THIRD_PARTY_DIR}/cpp-httplib/httplib.h")
    message(FATAL_ERROR "third_party submodules missing in octibot")
endif()

add_library(octibot STATIC
    sdk/src/OctiBotConfig.cxx
    sdk/src/OctiBotPolicy.cxx
    sdk/src/OctiBotRunner.cxx
    sdk/src/OctiBotMcpServer.cxx
)

target_include_directories(octibot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_BINARY_DIR}/core
)

target_include_directories(octibot SYSTEM PUBLIC
    ${OCTIBOT_THIRD_PARTY_DIR}/json/include
    ${OCTIBOT_THIRD_PARTY_DIR}/cpp-httplib
)

target_link_libraries(octibot PUBLIC
    octi_core
)

# Reference Bot Executable
add_executable(octibot-reference
    examples/reference_bot/Main.cxx
)
target_link_libraries(octibot-reference PRIVATE
    octibot
    octi_core
)

# Minimal Starter Bot Executable
add_executable(octibot-minimal
    examples/minimal_bot/Main.cxx
)
target_link_libraries(octibot-minimal PRIVATE
    octibot
    octi_core
)

# Centaur MCP Bot Executable
add_executable(octibot-mcp
    examples/reference_bot/Main.cxx
)
target_compile_definitions(octibot-mcp PRIVATE OCTIBOT_MCP_DEFAULT=1)
target_link_libraries(octibot-mcp PRIVATE
    octibot
    octi_core
)

# Qualification Test Suite
if(OCTIBOT_BUILD_TESTS AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CPPUTEST REQUIRED cpputest)
    find_library(RAPIDCHECK_LIBRARY rapidcheck REQUIRED)

    # Link in-process octi_server_lib so loopback server tests run hermetically
    set(OCTI_SERVER_SANITIZE ${OCTIBOT_SANITIZE} CACHE BOOL "" FORCE)
    set(OCTI_SERVER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../octi/server ${CMAKE_CURRENT_BINARY_DIR}/octi_server)

    add_executable(test_octibot_sdk
        test/TestOctiBotSdk.cxx
    )

    target_include_directories(test_octibot_sdk PRIVATE
        ${CPPUTEST_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../octi/server/include
    )

    target_link_directories(test_octibot_sdk PRIVATE
        ${CPPUTEST_LIBRARY_DIRS}
    )

    target_link_libraries(test_octibot_sdk PRIVATE
        octibot
        octi_server_lib
        octi_core
        ${CPPUTEST_LIBRARIES}
        ${RAPIDCHECK_LIBRARY}
    )
endif()

cmake_minimum_required(VERSION 3.16)

# Extract version from root README.md ("**Version: major.minor.patch**")
set(_ROOT_README_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ROOT_README_FILE}")
file(READ "${_ROOT_README_FILE}" _ROOT_README_CONTENT)
string(REGEX MATCH "\\*\\*Version:[ \t]*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\*\\*" _VER_MATCH "${_ROOT_README_CONTENT}")
if(NOT _VER_MATCH)
    message(FATAL_ERROR "Could not extract **Version: x.y.z** from ${_ROOT_README_FILE}")
endif()
set(PROJ_VER "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

project(octi_core VERSION ${PROJ_VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -UNDEBUG)
endif()

option(OCTI_CORE_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(OCTI_CORE_BUILD_TESTS "Build octi_core CppUTest suite" ON)

if(OCTI_CORE_SANITIZE AND NOT MSVC AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer
                        -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

set(WHOAMI_OUTPUT)
execute_process(
    COMMAND whoami
    OUTPUT_VARIABLE WHOAMI_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "^.*\\\\" "" WHOAMI_OUTPUT "${WHOAMI_OUTPUT}")

set(HOSTNAME_OUTPUT)
execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE HOSTNAME_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP DATE_OUTPUT "%Y-%m-%d %H:%M:%S")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.hxx.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.hxx
    @ONLY
)

set(OCTI_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(NOT EXISTS "${OCTI_THIRD_PARTY_DIR}/json/include/nlohmann/json.hpp" OR
   NOT EXISTS "${OCTI_THIRD_PARTY_DIR}/cpp-httplib/httplib.h")
    message(FATAL_ERROR
        "third_party submodules missing; run: git submodule update --init --recursive")
endif()

add_library(octi_core STATIC
    src/OctiCore.cxx
    src/OctiCrypto.cxx
    src/OctiPod.cxx
    src/OctiSquare.cxx
    src/OctiMove.cxx
    src/OctiBoard.cxx
    src/OctiGame.cxx
    src/OctiBotEngine.cxx
    src/OctiNetworkClient.cxx
)

target_include_directories(octi_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(octi_core SYSTEM PUBLIC
    ${OCTI_THIRD_PARTY_DIR}/json/include
    ${OCTI_THIRD_PARTY_DIR}/cpp-httplib
)

if(WIN32)
    target_link_libraries(octi_core PUBLIC
        ws2_32
        crypt32
        winhttp
    )
else()
    find_package(Threads REQUIRED)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(octi_core PUBLIC
        CPPHTTPLIB_OPENSSL_SUPPORT
    )
    target_link_libraries(octi_core PUBLIC
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()

add_executable(octi_book_gen
    src/OctiBookGenMain.cxx
)

target_link_libraries(octi_book_gen PRIVATE
    octi_core
)

if(OCTI_CORE_BUILD_TESTS AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CPPUTEST REQUIRED cpputest)
    find_library(RAPIDCHECK_LIBRARY rapidcheck REQUIRED)

    add_executable(test_octi_core
        test/TestOctiCore.cxx
    )

    target_include_directories(test_octi_core PRIVATE
        ${CPPUTEST_INCLUDE_DIRS}
    )

    target_link_directories(test_octi_core PRIVATE
        ${CPPUTEST_LIBRARY_DIRS}
    )

    target_link_libraries(test_octi_core PRIVATE
        octi_core
        ${CPPUTEST_LIBRARIES}
        ${RAPIDCHECK_LIBRARY}
    )
endif()

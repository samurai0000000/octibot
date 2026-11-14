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

option(OCTI_BOOTSTRAP_SOUND_ASSETS "Allow configuring before assets/sounds/*.wav exist" OFF)

set(_OCTI_SOUND_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/sounds")
set(_OCTI_SOUND_CUE_IDS
    pod_place
    prong_snap
    pod_step
    pod_jump
    pod_capture
    clock_low_tick
    game_victory
)

set(_OCTI_SOUND_ASSETS_HXX_TMP "${CMAKE_CURRENT_BINARY_DIR}/OctiSoundAssets.hxx.tmp")
set(_OCTI_SOUND_ASSETS_HXX_OUT "${CMAKE_CURRENT_BINARY_DIR}/OctiSoundAssets.hxx")
set(_OCTI_SOUND_ASSETS_BUF
"/*
 * OctiSoundAssets.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_SOUND_ASSETS_HXX
#define OCTI_SOUND_ASSETS_HXX

#include <cstddef>
#include <cstdint>

namespace octi {
"
)

foreach(_cue_id IN LISTS _OCTI_SOUND_CUE_IDS)
    set(_wav_path "${_OCTI_SOUND_ASSET_DIR}/${_cue_id}.wav")
    if(EXISTS "${_wav_path}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_wav_path}")
        file(READ "${_wav_path}" _wav_hex HEX)
        string(LENGTH "${_wav_hex}" _hex_len)
        math(EXPR _byte_len "${_hex_len} / 2")
        if(_byte_len EQUAL 0)
            message(FATAL_ERROR "Sound asset is empty: ${_wav_path}")
        endif()
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " _c_bytes "${_wav_hex}")
        string(REGEX REPLACE "((0x[0-9a-f][0-9a-f], ){12})" "\\1\n    " _c_bytes_wrapped "${_c_bytes}")
        string(APPEND _OCTI_SOUND_ASSETS_BUF
"\nstatic constexpr std::size_t kOctiWavSize_${_cue_id} = ${_byte_len}u;\n"
"static constexpr std::uint8_t kOctiWav_${_cue_id}[${_byte_len}] = {\n"
"    ${_c_bytes_wrapped}\n"
"};\n"
        )
    elseif(OCTI_BOOTSTRAP_SOUND_ASSETS)
        string(APPEND _OCTI_SOUND_ASSETS_BUF
"\nstatic constexpr std::size_t kOctiWavSize_${_cue_id} = 0u;\n"
"static constexpr std::uint8_t kOctiWav_${_cue_id}[1] = { 0x00 };\n"
        )
    else()
        message(FATAL_ERROR
            "Missing canonical sound asset: ${_wav_path}; run: make sound-assets")
    endif()
endforeach()

string(APPEND _OCTI_SOUND_ASSETS_BUF
"
} // namespace octi

#endif // OCTI_SOUND_ASSETS_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: \"BSD\"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
"
)

file(WRITE "${_OCTI_SOUND_ASSETS_HXX_TMP}" "${_OCTI_SOUND_ASSETS_BUF}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_OCTI_SOUND_ASSETS_HXX_TMP}"
        "${_OCTI_SOUND_ASSETS_HXX_OUT}"
)
file(REMOVE "${_OCTI_SOUND_ASSETS_HXX_TMP}")

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
    src/OctiSoundCue.cxx
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

if(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
    add_executable(octi_sound_gen
        src/OctiSoundGenMain.cxx
    )

    target_include_directories(octi_sound_gen PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

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

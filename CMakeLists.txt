# SPDX-License-Identifier: GPL-3.0-only
# Elasticat FM-1, native build: the library (every unit of src/, with the native file API stand-in), the harness
# host/ec_run and the tests. The .rfm1 is built by FM1VST (core-glue/elasticat), from the same sources.
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.20)
project(elasticatfm1 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)          # (gnu99: __atomic builtins, POSIX calls in the native stand-in)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
find_package(Python3 COMPONENTS Interpreter)

# every unit of the engine; src/dsp and src/seq join as they appear (written in parallel: E0 does not depend on them)
file(GLOB EC_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/io/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/src/seq/*.c)
add_library(elasticat STATIC ${EC_SOURCES})
# src/plat/abi: copies of FM1VST's core-api/fm1core.h and core-glue/common/rfm1_host.h (FM1VST's own are used there)
target_include_directories(elasticat PUBLIC src/plat src/plat/abi src/io src/ui src/dsp src/seq)
target_compile_definitions(elasticat PUBLIC EC_FS_NATIVE)
target_compile_options(elasticat PRIVATE -Wall -Wextra -ffp-contract=off)
target_link_libraries(elasticat PUBLIC m)

add_executable(ec_run host/ec_run.c host/ec_png.c)
target_link_libraries(ec_run PRIVATE elasticat)
target_compile_options(ec_run PRIVATE -Wall -Wextra)

add_executable(ec_test_io host/test_io.c)
target_link_libraries(ec_test_io PRIVATE elasticat)
target_compile_options(ec_test_io PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME io COMMAND ec_test_io ${CMAKE_BINARY_DIR}/test_io_folder)
# the E0 smoke run: the test loop generated into a folder, booted, played, the screen and audio written
if(Python3_FOUND)
    set(_tf ${CMAKE_BINARY_DIR}/test_folder)
    add_test(NAME e0_testloop
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/host/make_testloop.py
                     ${_tf}/audio/testloop_bpm120.wav)
    add_test(NAME e0_run
             COMMAND ec_run --folder ${_tf} --ms 6000 --press PLAY@1000 --turn K1@2000:3 --key 9@2500
                     --rms-from 1200 --min-rms 0.02 --expect-ui file:testloop_bpm120.wav --expect-ui load:done
                     --expect-ui play:1 --wav ${CMAKE_BINARY_DIR}/e0_run.wav --png ${CMAKE_BINARY_DIR}/e0_run.png)
    set_tests_properties(e0_run PROPERTIES DEPENDS e0_testloop)
endif()
add_test(NAME e0_nofolder
         COMMAND ec_run --ms 3000 --press PLAY@500 --rms-from 800 --min-rms 0.02 --expect-ui scan:nofolder
                 --expect-ui "file:BUILTIN LOOP")

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
     ${CMAKE_CURRENT_SOURCE_DIR}/src/seq/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.c)
add_library(elasticat STATIC ${EC_SOURCES})

# The layout: the pages are data. design/layout/elasticat.json -> src/app/ec_layout_gen.c (committed: a build without
# Python uses it as it is), the tests' fixed layout host/fixtures/layout-0.4.0.json -> host/ec_layout_test_gen.c, the
# per-page knob tables in docs/FM1-CONTROLS.md; re-run when a design, the generator or the param table changes
set(_lay_gen ${CMAKE_CURRENT_SOURCE_DIR}/tools/layout/gen_layout.py)
set(_lay_deps ${_lay_gen} ${CMAKE_CURRENT_SOURCE_DIR}/src/seq/ec_params.c ${CMAKE_CURRENT_SOURCE_DIR}/src/seq/ec_params.h)
if(Python3_FOUND)
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ec_layout.stamp
        COMMAND ${Python3_EXECUTABLE} ${_lay_gen} ${CMAKE_CURRENT_SOURCE_DIR}/design/layout/elasticat.json
                --c ${CMAKE_CURRENT_SOURCE_DIR}/src/app/ec_layout_gen.c
                --docs ${CMAKE_CURRENT_SOURCE_DIR}/docs/FM1-CONTROLS.md
        COMMAND ${Python3_EXECUTABLE} ${_lay_gen} ${CMAKE_CURRENT_SOURCE_DIR}/host/fixtures/layout-0.4.0.json --quiet --no-desc
                --c ${CMAKE_CURRENT_SOURCE_DIR}/host/ec_layout_test_gen.c --symbol EC_LAYOUT_TEST
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/ec_layout.stamp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/design/layout/elasticat.json ${CMAKE_CURRENT_SOURCE_DIR}/design/layout/descriptions.json
                ${CMAKE_CURRENT_SOURCE_DIR}/host/fixtures/layout-0.4.0.json ${_lay_deps}
        BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/src/app/ec_layout_gen.c ${CMAKE_CURRENT_SOURCE_DIR}/host/ec_layout_test_gen.c
        COMMENT "gen_layout: design/layout/elasticat.json -> src/app/ec_layout_gen.c" VERBATIM)
    add_custom_target(ec_layout_gen DEPENDS ${CMAKE_BINARY_DIR}/ec_layout.stamp)
    add_dependencies(elasticat ec_layout_gen)
endif()
# src/plat/abi: copies of FM1VST's core-api/fm1core.h and core-glue/common/rfm1_host.h (FM1VST's own are used there)
target_include_directories(elasticat PUBLIC src/plat src/plat/abi src/io src/ui src/dsp src/seq src/app)
target_compile_definitions(elasticat PUBLIC EC_FS_NATIVE)
target_compile_options(elasticat PRIVATE -Wall -Wextra -ffp-contract=off)
target_link_libraries(elasticat PUBLIC m)

# the tests' fixed layout (0.4.0's pages): every host program links it, the tests and demo scripts select it
add_library(ec_testlayout STATIC host/ec_layout_test_gen.c)
target_link_libraries(ec_testlayout PUBLIC elasticat)
target_include_directories(ec_testlayout PUBLIC host)
if(TARGET ec_layout_gen)
    add_dependencies(ec_testlayout ec_layout_gen)
endif()
link_libraries(ec_testlayout)

add_executable(ec_run host/ec_run.c host/ec_png.c)
target_link_libraries(ec_run PRIVATE elasticat)
target_compile_options(ec_run PRIVATE -Wall -Wextra)

add_executable(ec_test_io host/test_io.c)
target_link_libraries(ec_test_io PRIVATE elasticat)
target_compile_options(ec_test_io PRIVATE -Wall -Wextra)

add_executable(ec_test_seq host/test_seq.c)
target_link_libraries(ec_test_seq PRIVATE elasticat)
target_compile_options(ec_test_seq PRIVATE -Wall -Wextra)

add_executable(ec_test_e3 host/test_e3.c)   # E3: razor, slice events, voicing, the RZR1 chunk
target_link_libraries(ec_test_e3 PRIVATE elasticat)
target_include_directories(ec_test_e3 PRIVATE src/dsp)
target_compile_options(ec_test_e3 PRIVATE -Wall -Wextra)
add_executable(ec_test_seq_e4 host/test_seq_e4.c)                  # E4
target_link_libraries(ec_test_seq_e4 PRIVATE elasticat)
target_compile_options(ec_test_seq_e4 PRIVATE -Wall -Wextra)
add_executable(ec_make_heavy host/make_heavy.c)   # the heaviest project, for CPU runs (README: CPU)
target_link_libraries(ec_make_heavy PRIVATE elasticat)
target_compile_options(ec_make_heavy PRIVATE -Wall -Wextra)
add_executable(ec_test_v020 host/test_v020.c)   # 0.2.0: the E3 / E4 / E5 seams through the real chain
target_link_libraries(ec_test_v020 PRIVATE elasticat)
target_compile_options(ec_test_v020 PRIVATE -Wall -Wextra)
add_executable(ec_test_e5 host/test_e5.c)        # E5
target_link_libraries(ec_test_e5 PRIVATE elasticat)
target_compile_options(ec_test_e5 PRIVATE -Wall -Wextra)
add_executable(ec_test_ux host/test_ux.c)   # 0.3.0: the panel along ChoralRoot / FIZZ, the live labels
target_link_libraries(ec_test_ux PRIVATE elasticat)
target_compile_options(ec_test_ux PRIVATE -Wall -Wextra)

add_executable(ec_test_e6 host/test_e6.c)   # E6: audio input, the INPUT machine, sampling (REC)
target_link_libraries(ec_test_e6 PRIVATE elasticat)
target_compile_options(ec_test_e6 PRIVATE -Wall -Wextra)

add_executable(ec_test_mtx host/test_mtx.c host/ec_png.c)   # 0.5.0: the mod matrix, MOD KEYS, migration
target_link_libraries(ec_test_mtx PRIVATE elasticat)
target_compile_options(ec_test_mtx PRIVATE -Wall -Wextra)

add_executable(ec_mock_ux2 host/mock_ux2.c host/ec_png.c)   # 0.6.0 mockups (tools/mockups/README.md); no test
target_link_libraries(ec_mock_ux2 PRIVATE elasticat)
target_compile_options(ec_mock_ux2 PRIVATE -Wall -Wextra)
add_executable(ec_test_pack tools/layout/test_pack.c tools/layout/ec_layout_pack.c)   # the designer's load path
target_include_directories(ec_test_pack PRIVATE tools/layout)
target_compile_options(ec_test_pack PRIVATE -Wall -Wextra)
add_executable(ec_test_layout host/test_layout.c)   # the shipped layout through panel input, params by id
target_compile_options(ec_test_layout PRIVATE -Wall -Wextra)
add_executable(ec_dump_pages host/dump_pages.c)   # every page in every context: the 0.4.0 proof below
target_compile_options(ec_dump_pages PRIVATE -Wall -Wextra)

enable_testing()
# The layout: the tests' fixed layout gives exactly 0.4.0's pages (the hash of every page in every context, recorded from
# 0.4.0's hand-written ec_pages.c), and the designs pass the generator's checks
add_test(NAME layout_040 COMMAND sh -c "$<TARGET_FILE:ec_dump_pages> --layout test --ncat 9 | tail -1 | diff - ${CMAKE_CURRENT_SOURCE_DIR}/host/fixtures/pages-0.4.0.hash")
if(Python3_FOUND)
    add_test(NAME layout_walk COMMAND sh -c "python3 ${CMAKE_CURRENT_SOURCE_DIR}/host/make_demo_folder.py ${CMAKE_BINARY_DIR}/layout_folder >/dev/null && $<TARGET_FILE:ec_test_layout> ${CMAKE_BINARY_DIR}/layout_folder")
    add_test(NAME layout_check COMMAND ${Python3_EXECUTABLE} ${_lay_gen} ${CMAKE_CURRENT_SOURCE_DIR}/design/layout/elasticat.json --check)
    add_test(NAME layout_pack COMMAND sh -c "${Python3_EXECUTABLE} ${_lay_gen} ${CMAKE_CURRENT_SOURCE_DIR}/design/layout/elasticat.json --quiet --pack ${CMAKE_BINARY_DIR}/shipped.pack && $<TARGET_FILE:ec_test_pack> ${CMAKE_BINARY_DIR}/shipped.pack")
endif()
add_test(NAME e5 COMMAND ec_test_e5 ${CMAKE_CURRENT_SOURCE_DIR}/host/fixtures/e2_flash.bin)   # E5
add_test(NAME v020 COMMAND ec_test_v020)
# 0.3.0: the new screens through panel input (host/render_ux.sh -> results/ux)
add_test(NAME ux_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_ux.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/ux)
# 0.3.0: lanes, setup, PRESETS = the sample, knob speed / snap, F3, pop-ups, track keys, lock clear, SHIFT, labels
add_test(NAME ux COMMAND sh -c "python3 ${CMAKE_CURRENT_SOURCE_DIR}/host/make_demo_folder.py ${CMAKE_BINARY_DIR}/ux_folder >/dev/null && $<TARGET_FILE:ec_test_ux> ${CMAKE_BINARY_DIR}/ux_folder")
# 0.2.0: the seams demo through panel input + a reboot from its flash (host/render_v020.sh)
add_test(NAME v020_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_v020.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/v020)
# E6: the INPUT machine's alignment, no input, the REC pop-up, a synced take through a latency, resample, threshold,
# the WAV in /data/rec and the reboot
add_test(NAME e6 COMMAND sh -c "python3 ${CMAKE_CURRENT_SOURCE_DIR}/host/make_demo_folder.py ${CMAKE_BINARY_DIR}/e6_folder >/dev/null && $<TARGET_FILE:ec_test_e6> ${CMAKE_BINARY_DIR}/e6_folder")
# E6: the REC / INPUT screens through panel input with a stand-in input (host/render_e6.sh -> results/e6)
add_test(NAME e6_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_e6.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/e6)
# 0.5.0: the mod matrix (resolve, migration, the MATRIX page, MOD KEYS) on the shipped layout
add_test(NAME mtx COMMAND sh -c "python3 ${CMAKE_CURRENT_SOURCE_DIR}/host/make_demo_folder.py ${CMAKE_BINARY_DIR}/mtx_folder >/dev/null && $<TARGET_FILE:ec_test_mtx> ${CMAKE_BINARY_DIR}/mtx_folder")
add_test(NAME io COMMAND ec_test_io ${CMAKE_BINARY_DIR}/test_io_folder)
add_test(NAME seq COMMAND ec_test_seq)
add_test(NAME e3 COMMAND ec_test_e3)
# E3: the demo through panel input (razor, warp pages, neighbor) + a reboot from its flash
add_test(NAME e3_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_e3.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/e3)
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
# E1/E2: the demo built through panel input only, the reboot from its saved flash, a missing sample
add_test(NAME e1_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_e1.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/e1)
# E5: scenes, copy / paste, PAGE, naming, memorize / recall, undo, slice programs through panel input; the reboot;
# an E2 save
add_test(NAME e5_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_e5.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/e5)
add_test(NAME e0_nofolder
         COMMAND ec_run --ms 3000 --press PLAY@500 --rms-from 800 --min-rms 0.02 --expect-ui scan:nofolder
                 --expect-ui "file:BUILTIN LOOP")
# E4: FX on every slot, the sends, the master insert and MOD through panel input (results/e4); the model side
add_test(NAME seq_e4 COMMAND ec_test_seq_e4)
add_test(NAME e4_demo COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/host/render_e4.sh ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/e4)
# 0.6.0: the graphics rules (docs/UX-V2.md §7) on every screen reached by state, in all six themes: every 2x2 block of
# the LCD one colour, every colour one of the theme's 16 (the CREAM frames -> build/grid for a look)
add_executable(ec_test_grid host/test_grid.c host/ec_png.c)
target_link_libraries(ec_test_grid PRIVATE elasticat)
target_compile_options(ec_test_grid PRIVATE -Wall -Wextra)
add_test(NAME grid COMMAND sh -c "python3 ${CMAKE_CURRENT_SOURCE_DIR}/host/make_demo_folder.py ${CMAKE_BINARY_DIR}/grid_folder >/dev/null && mkdir -p ${CMAKE_BINARY_DIR}/grid && $<TARGET_FILE:ec_test_grid> ${CMAKE_BINARY_DIR}/grid_folder ${CMAKE_BINARY_DIR}/grid")
# 0.6.0 (data): the project record version 2 (sizes, old records, 64 patterns / 128 slots), micro timing, lock-only
# steps, the live-recording timing (quantize, beats, count-in)
add_executable(ec_test_rec host/test_rec.c)
target_link_libraries(ec_test_rec PRIVATE elasticat)
target_compile_options(ec_test_rec PRIVATE -Wall -Wextra)
add_test(NAME rec COMMAND ec_test_rec ${CMAKE_CURRENT_SOURCE_DIR}/host/fixtures)

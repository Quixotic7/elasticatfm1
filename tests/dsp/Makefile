# Standalone build of the DSP tests (independent of the main CMake).
#   make -C tests/dsp          build + run the unit tests
#   make -C tests/dsp render   write the demo WAVs to results/dsp/
#   make -C tests/dsp bench    native benchmark (6 busy tracks)
#   make -C tests/dsp wasm     check the engine compiles with emcc (wasm32, mvp)
# Run from the repo root or via -C; outputs go to <repo>/results/dsp.

ROOT   := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
CC     ?= clang
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wshadow -D_POSIX_C_SOURCE=200112L
SRC    := $(wildcard $(ROOT)/src/dsp/*.c)
HDR    := $(wildcard $(ROOT)/src/dsp/*.h) $(wildcard $(ROOT)/tests/dsp/*.h)
OUT    := $(ROOT)/build-dsp
EMCC   ?= $(HOME)/emsdk/upstream/emscripten/emcc

.PHONY: all test render bench wasm clean

all: test

$(OUT):
	mkdir -p $(OUT) $(ROOT)/results/dsp

$(OUT)/test_dsp: $(ROOT)/tests/dsp/test_dsp.c $(SRC) $(HDR) | $(OUT)
	$(CC) $(CFLAGS) -o $@ $(ROOT)/tests/dsp/test_dsp.c $(SRC) -lm

$(OUT)/render_demo: $(ROOT)/tests/dsp/render_demo.c $(SRC) $(HDR) | $(OUT)
	$(CC) $(CFLAGS) -o $@ $(ROOT)/tests/dsp/render_demo.c $(SRC) -lm

$(OUT)/bench: $(ROOT)/tests/dsp/bench.c $(SRC) $(HDR) | $(OUT)
	$(CC) $(CFLAGS) -o $@ $(ROOT)/tests/dsp/bench.c $(SRC) -lm

test: $(OUT)/test_dsp
	cd $(ROOT) && $(OUT)/test_dsp

render: $(OUT)/render_demo
	cd $(ROOT) && $(OUT)/render_demo

bench: $(OUT)/bench
	cd $(ROOT) && $(OUT)/bench

wasm: | $(OUT)
	$(EMCC) -std=c99 -O2 -Wall -Wextra -mcpu=mvp -c $(SRC) && rm -f ec_*.o && echo "emcc: src/dsp compiles for wasm32"

clean:
	rm -f $(OUT)/test_dsp $(OUT)/render_demo $(OUT)/bench

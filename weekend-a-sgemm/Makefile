# Weekend A scaffolding.
#
# Each kernels/<name>.c defines sgemm() (see harness/sgemm.h) and becomes
# build/<name>, linked with the timing and verification harness.
# Each bench/<name>.c is a standalone program and becomes build/bench/<name>.
#
#   make                                build everything
#   make OPT=-O3 ARCH=-march=native     flags are tracked: changing them rebuilds
#   make EXTRA=-fopt-info-vec           see what the compiler vectorized (A4)
#   make EXTRA=-fopenmp                 OpenMP (A5)
#   make check                          sanitizer builds, run on small odd sizes
#   make perf K=naive ARGS="-r 1 1024"  perf stat on one kernel
#   make clean

CC    := gcc
OPT   ?= -O2
ARCH  ?=
EXTRA ?=

WARN     := -std=gnu17 -Wall -Wextra -Wpedantic
CFLAGS   := $(WARN) $(OPT) $(ARCH) $(EXTRA) -g
SANFLAGS := $(WARN) -O1 -g $(ARCH) $(EXTRA) -fsanitize=address,undefined \
            -fno-sanitize-recover=all -fno-omit-frame-pointer
HFLAGS   := $(WARN) -O2 -g
LDLIBS   := -lm
# bench/ measures memory: stop GCC turning copy/fill loops into memcpy/memset.
BENCH_FLAGS := -fno-tree-loop-distribute-patterns

KERNELS := $(basename $(notdir $(wildcard kernels/*.c)))
BENCHES := $(basename $(notdir $(wildcard bench/*.c)))
SOURCES := harness/harness.c $(wildcard kernels/*.c bench/*.c)

PERF_EVENTS ?= cycles,instructions,cache-misses,LLC-load-misses,L1-dcache-load-misses
CHECK_SIZES := 1 2 3 17 64 100 127

.PHONY: all check perf clean FORCE
.DELETE_ON_ERROR:

all: $(KERNELS:%=build/%) $(BENCHES:%=build/bench/%) compile_commands.json
	$(if $(KERNELS),,@echo "No kernels yet: add kernels/<name>.c defining sgemm() from harness/sgemm.h")

# Flag files change only when the flags do, so objects rebuild after a flag change.
build/flags: FORCE
	@mkdir -p $(@D)
	@echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
build/san/flags: FORCE
	@mkdir -p $(@D)
	@echo '$(SANFLAGS)' | cmp -s - $@ || echo '$(SANFLAGS)' > $@
FORCE:

# The harness always builds with fixed flags, so OPT/ARCH/EXTRA affect only kernels.
build/harness.o: harness/harness.c harness/sgemm.h
	@mkdir -p $(@D)
	$(CC) $(HFLAGS) -c $< -o $@

build/obj/%.o: kernels/%.c harness/sgemm.h build/flags
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Iharness -c $< -o $@

$(addprefix build/,$(KERNELS)): build/%: build/obj/%.o build/harness.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

build/bench/%: bench/%.c build/flags
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(BENCH_FLAGS) $< -o $@ $(LDLIBS)

build/san/harness.o: harness/harness.c harness/sgemm.h build/san/flags
	@mkdir -p $(@D)
	$(CC) $(SANFLAGS) -c $< -o $@

build/san/obj/%.o: kernels/%.c harness/sgemm.h build/san/flags
	@mkdir -p $(@D)
	$(CC) $(SANFLAGS) -Iharness -c $< -o $@

$(addprefix build/san/,$(KERNELS)): build/san/%: build/san/obj/%.o build/san/harness.o
	$(CC) $(SANFLAGS) $^ -o $@ $(LDLIBS)

check: $(addprefix build/san/,$(KERNELS))
	@set -e; for k in $(KERNELS); do echo "== $$k"; ./build/san/$$k -r 1 $(CHECK_SIZES); done

ifneq ($(filter perf,$(MAKECMDGOALS)),)
ifeq ($(K),)
$(error perf needs K=<kernel>, e.g. make perf K=naive ARGS="-r 1 1024")
endif
endif

perf: build/$(K)
	perf stat -e $(PERF_EVENTS) ./build/$(K) $(ARGS)

# For clangd. Written by hand because bear isn't installed.
compile_commands.json: $(SOURCES) build/flags Makefile
	@{ echo '['; sep=' '; \
	   for f in $(SOURCES); do \
	     printf '%s {"directory": "%s", "file": "%s", "command": "$(CC) $(CFLAGS) -Iharness -c %s"}\n' \
	       "$$sep" "$(CURDIR)" "$$f" "$$f"; \
	     sep=','; \
	   done; echo ']'; } > $@

clean:
	rm -rf build compile_commands.json

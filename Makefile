# casim: computer algebra for the STU-32, portable C11, built on the abacus core's equation
# engine and number type. `make test` runs every check.
#
# The core is consumed at a pinned commit (CORE_PIN), exported read-only from a local clone of
# cpuchip/abacus-firmware with `git archive`, so a build never writes into that clone and never
# sees its uncommitted work. The core's own fetch script builds the Intel decimal library inside
# the export (sha256-pinned, BSD-3-Clause).
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CORE_REPO ?= ../abacus-firmware
CORE_PIN := $(shell cat CORE_PIN)
CB := build/core
INTEL_TGZ := third_party/intel-dfp/IntelRDFPMathLib20U5.tar.gz
INTEL_SRC := $(CB)/third_party/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src
BID_FLAGS := -DDECIMAL_CALL_BY_REFERENCE=0 -DDECIMAL_GLOBAL_ROUNDING=0 \
             -DDECIMAL_GLOBAL_EXCEPTION_FLAGS=0
CASIM_SRC := $(wildcard src/*.c)
CASIM_HDRS := $(wildcard src/*.h)
TESTS := $(patsubst tests/%.c,build/%,$(wildcard tests/*_test.c))
LIBS := build/libcore.a $(CB)/build/intel/libbid.a -lm
FRAME := -Werror=frame-larger-than=1024     # an error whatever CFLAGS says (review)

.PHONY: all test size clean
all: $(TESTS) build/casim

test: all build/frames.ok build/clang.ok build/symbols.ok build/gcdprimes_check
	scripts/run-tests.sh $(TESTS) build/gcdprimes_check

# CAS 010's gcd on small primes: one translation unit with src/casim.c (its prime list swapped), so not the TESTS rule
build/gcdprimes_check: tests/gcdprimes_check.c $(CASIM_SRC) $(CASIM_HDRS) build/core/.pin
	$(CC) $(CFLAGS) -I$(CB)/core -Isrc -Itests -o $@ $< $(LIBS)

# No pow in casim's objects (abacus #5589): libm's or libbid's would land in the firmware. A planted control must fail.
build/symbols.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/symbol_control.c scripts/symbol-check.sh build/core/.pin
	scripts/symbol-check.sh
	touch $@

# The site's wasm build is clang: casim's sources under clang -Wall -Wextra -Werror too (abacus #5430, after
# primer #5429), with a planted control that must fail.
build/clang.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/clang_control.c scripts/clang-check.sh build/core/.pin
	scripts/clang-check.sh
	touch $@

# CAS 004 rule 3: no frame in src/ larger than 1 KB, here as on the S3 (scripts/size-s3.sh). The planted
# control, a 2 KB frame, must fail the same flags, or the guard checks nothing.
build/frames.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/frame_control.c build/core/.pin Makefile
	for f in $(CASIM_SRC); do $(CC) $(CFLAGS) $(FRAME) -I$(CB)/core -Isrc -c -o /dev/null $$f || exit 1; done
	if $(CC) $(CFLAGS) $(FRAME) -c -o /dev/null tests/frame_control.c 2>/dev/null; then \
	    echo "frame guard: the 2 KB control compiled, so the guard checks nothing"; exit 1; fi
	touch $@

# S3 code size of casim's own sources (the core's is measured in its own repo).
size: build/core/.pin
	scripts/size-s3.sh

# The pinned core, exported fresh whenever CORE_PIN changes. The tarball is seeded from the
# core clone when it has one; the fetch script verifies it against its pin either way.
build/core/.pin: CORE_PIN
	rm -rf $(CB) && mkdir -p $(CB)
	git -C $(CORE_REPO) archive $(CORE_PIN) | tar -x -C $(CB)
	if [ -f $(CORE_REPO)/$(INTEL_TGZ) ]; then mkdir -p $(CB)/third_party/intel-dfp && cp $(CORE_REPO)/$(INTEL_TGZ) $(CB)/$(INTEL_TGZ); fi
	CC="$(CC)" $(CB)/scripts/fetch-intel-dfp.sh
	echo $(CORE_PIN) > $@

# The core's sources as its own Makefile lists them (CORE :=), and its engine, in one archive.
build/libcore.a: build/core/.pin
	rm -rf build/coreobj && mkdir -p build/coreobj
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o build/coreobj/abn_intel.o $(CB)/core/abn_intel.c
	for f in $$(sed -n 's/^CORE := //p' $(CB)/Makefile); do \
	    $(CC) $(CFLAGS) -I$(CB)/core -c -o build/coreobj/$$(basename $$f .c).o $(CB)/$$f || exit 1; done
	rm -f $@ && ar rcs $@ build/coreobj/*.o

build/%_test: tests/%_test.c tests/check.h tests/coreeval.h $(CASIM_SRC) $(CASIM_HDRS) build/libcore.a
	$(CC) $(CFLAGS) -I$(CB)/core -Isrc -Itests -o $@ $< $(CASIM_SRC) $(LIBS)

# the host command line abacus runs the hidden sets through
build/casim: cli/casim_cli.c $(CASIM_SRC) $(CASIM_HDRS) build/libcore.a
	$(CC) $(CFLAGS) -I$(CB)/core -Isrc -o $@ cli/casim_cli.c $(CASIM_SRC) $(LIBS)

clean:
	rm -rf build

# Casimir: computer algebra in portable C11 (README.md).
#
#   make               the library (build/libcasim.a), the command line (build/casim) and the C example; needs only
#                      this repository, a C11 compiler, and the network once for Intel's decimal library
#   make check         the standalone checks, as CI runs them: the golden corpus, the frame and symbol gates,
#                      the small-primes gcd check, check-abn, and docs/language.md's examples
#   make test          check, plus the developer tests: they read every answer back through the calculator
#                      firmware's own evaluator, so they need its clone (CORE_REPO) at CORE_PIN
#   make check-abn     the vendored number layer is the firmware's, unedited (third_party/abn/PIN)
#   make wasm          the WebAssembly module and its JavaScript wrapper (emscripten; README.md)
#
# Intel's library is fetched by URL and SHA-256 (scripts/fetch-intel-dfp.sh, BSD-3-Clause) and built with its own
# makefile; libcasim.a carries it, so a program links one archive and -lm.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
ABN := third_party/abn
INTEL_SRC := build/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src
BID_FLAGS := -DDECIMAL_CALL_BY_REFERENCE=0 -DDECIMAL_GLOBAL_ROUNDING=0 -DDECIMAL_GLOBAL_EXCEPTION_FLAGS=0
LIB := build/libcasim.a
CASIM_SRC := src/casim.c
CASIM_HDRS := src/casim.h $(ABN)/abn.h
# no frame above 1 KB, an error whatever CFLAGS says (review); clang spells it differently from gcc
FRAME := $(if $(shell $(CC) --version 2>/dev/null | grep -i clang),-Wframe-larger-than=1024 -Werror=frame-larger-than,\
                                                                  -Werror=frame-larger-than=1024)

# The developer tests' oracle: the firmware's core at CORE_PIN, exported read-only from a local clone with
# `git archive`, so a build never writes into that clone and never sees its uncommitted work.
CORE_REPO ?= ../stu32-firmware
CORE_PIN := $(shell cat CORE_PIN)
CB := build/core
CORE_INTEL_TGZ := third_party/intel-dfp/IntelRDFPMathLib20U5.tar.gz
CORE_INTEL_SRC := $(CB)/third_party/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src
TESTS := $(patsubst tests/%.c,build/%,$(wildcard tests/*_test.c))
CORE_LIBS := build/libcore.a $(CB)/build/intel/libbid.a -lm

.PHONY: all check test check-abn size wasm clean
all: $(LIB) build/casim build/hello build/golden_check

check: all build/frames.ok build/inline.ok build/symbols.ok build/gcdprimes_check build/pfcheck_check build/ircheck_check \
       build/ibcheck_check build/smcheck_check check-abn
	scripts/run-tests.sh build/gcdprimes_check build/pfcheck_check build/ircheck_check build/ibcheck_check \
	    build/smcheck_check
	scripts/frame-table.py
	build/golden_check tests/golden/corpus.txt
	scripts/golden-control.sh
	scripts/doc-examples.py
	scripts/log-values.py
	scripts/step-values.py
	scripts/istep-values.py
	scripts/ssteps-values.py
	scripts/partfrac-values.py
	scripts/psteps-values.py
	scripts/taylor-values.py
	scripts/limit-values.py
	scripts/integrate-values.py
	scripts/sum-values.py
	@test "$$(scripts/ci-test.sh --ref)" = "$(CORE_PIN)" || { \
	    echo "FAIL scripts/ci-test.sh: CI's make test would fetch $$(scripts/ci-test.sh --ref), not CORE_PIN (CAS 016)"; exit 1; }

test: check build/clang.ok $(TESTS)
	scripts/run-tests.sh $(TESTS)

# ---- the library ----

build/intel/libbid.a: scripts/fetch-intel-dfp.sh
	CC="$(CC)" scripts/fetch-intel-dfp.sh

build/abn_intel.o: $(ABN)/abn_intel.c $(ABN)/abn.h build/intel/libbid.a
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o $@ $<

build/casim.o: $(CASIM_SRC) $(CASIM_HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FRAME) -I$(ABN) -c -o $@ $<

# casim, abn and Intel's objects in one archive (the linker takes only the objects a program needs)
$(LIB): build/casim.o build/abn_intel.o build/intel/libbid.a
	rm -rf build/libobj && mkdir -p build/libobj && cd build/libobj && $(AR) x ../intel/libbid.a
	cp build/casim.o build/abn_intel.o build/libobj/
	rm -f $@ && $(AR) rcs $@ build/libobj/*.o

# the command line abacus runs the hidden sets through
build/casim: cli/casim_cli.c src/casim.h $(LIB)
	$(CC) $(CFLAGS) -Isrc -o $@ $< $(LIB) -lm

build/hello: examples/hello.c src/casim.h $(LIB)
	$(CC) $(CFLAGS) -Isrc -o $@ $< $(LIB) -lm

build/golden_check: tests/golden_check.c src/casim.h $(LIB)
	$(CC) $(CFLAGS) -Isrc -o $@ $< $(LIB) -lm

# ---- the standalone checks ----

check-abn:
	scripts/check-abn.sh
	scripts/check-abn-control.sh

# CAS 010's gcd on small primes: one translation unit with src/casim.c (its prime list swapped)
build/gcdprimes_check: tests/gcdprimes_check.c $(CASIM_SRC) $(CASIM_HDRS) build/abn_intel.o
	$(CC) $(CFLAGS) -I$(ABN) -Isrc -Itests -o $@ $< build/abn_intel.o build/intel/libbid.a -lm

# CAS 012's rule 7 check, reached by spoiling a numerator: one translation unit with src/casim.c
build/pfcheck_check: tests/pfcheck_check.c $(CASIM_SRC) $(CASIM_HDRS) build/abn_intel.o
	$(CC) $(CFLAGS) -I$(ABN) -Isrc -Itests -o $@ $< build/abn_intel.o build/intel/libbid.a -lm

# CAS 013's check and refusals, reached by spoiling a term or taking the check away: one translation unit with src/casim.c
build/ircheck_check: tests/ircheck_check.c $(CASIM_SRC) $(CASIM_HDRS) build/abn_intel.o
	$(CC) $(CFLAGS) -I$(ABN) -Isrc -Itests -o $@ $< build/abn_intel.o build/intel/libbid.a -lm

# CAS 017's substitution check, reached by spoiling G: one translation unit with src/casim.c
build/ibcheck_check: tests/ibcheck_check.c $(CASIM_SRC) $(CASIM_HDRS) build/abn_intel.o
	$(CC) $(CFLAGS) -I$(ABN) -Isrc -Itests -o $@ $< build/abn_intel.o build/intel/libbid.a -lm

# CAS 018's antidifference check, reached by spoiling F: one translation unit with src/casim.c
build/smcheck_check: tests/smcheck_check.c $(CASIM_SRC) $(CASIM_HDRS) build/abn_intel.o
	$(CC) $(CFLAGS) -I$(ABN) -Isrc -Itests -o $@ $< build/abn_intel.o build/intel/libbid.a -lm

# No pow in casim's objects (abacus #5589): libm's or libbid's would land in the firmware. A planted control must fail.
build/symbols.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/symbol_control.c scripts/symbol-check.sh
	scripts/symbol-check.sh
	touch $@

# CAS 004 rule 3: no frame in src/ larger than 1 KB, here as on the S3 (scripts/size-s3.sh). The planted
# control, a 2 KB frame, must fail the same flags, or the guard checks nothing.
build/frames.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/frame_control.c Makefile
	@mkdir -p build
	for f in $(CASIM_SRC); do $(CC) $(CFLAGS) $(FRAME) -I$(ABN) -Isrc -c -o /dev/null $$f || exit 1; done
	if $(CC) $(CFLAGS) $(FRAME) -c -o /dev/null tests/frame_control.c 2>/dev/null; then \
	    echo "frame guard: the 2 KB control compiled, so the guard checks nothing"; exit 1; fi
	touch $@

# derive's recursion keeps its frame: deriv() stays inline, with a planted noinline control (work/steps-derive.md)
build/inline.ok: $(CASIM_SRC) $(CASIM_HDRS) scripts/inline-check.sh
	CC="$(CC)" scripts/inline-check.sh
	touch $@

# The site's wasm build is clang: casim's sources under a pinned clang 18 -Wall -Wextra -Werror too (abacus #5430,
# after primer #5429), with a planted control that must fail. Needs docker; CI builds everything with clang instead.
build/clang.ok: $(CASIM_SRC) $(CASIM_HDRS) tests/clang_control.c scripts/clang-check.sh
	scripts/clang-check.sh
	touch $@

# ---- the developer tests (the firmware core as the oracle) ----

# The pinned core, exported fresh whenever CORE_PIN changes. The tarball is seeded from the core clone when it
# has one; the core's fetch script verifies it against its pin either way.
build/core/.pin: CORE_PIN
	@git -C $(CORE_REPO) cat-file -e $(CORE_PIN)^{commit} 2>/dev/null || { \
	    echo "make test needs the firmware's clone at $(CORE_REPO) (CORE_REPO=...); make check runs without it"; exit 1; }
	rm -rf $(CB) && mkdir -p $(CB)
	git -C $(CORE_REPO) archive $(CORE_PIN) | tar -x -C $(CB)
	if [ -f $(CORE_REPO)/$(CORE_INTEL_TGZ) ]; then mkdir -p $(CB)/third_party/intel-dfp && \
	    cp $(CORE_REPO)/$(CORE_INTEL_TGZ) $(CB)/$(CORE_INTEL_TGZ); fi
	CC="$(CC)" $(CB)/scripts/fetch-intel-dfp.sh
	echo $(CORE_PIN) > $@

# The core's sources as its own Makefile lists them (CORE :=), and its engine, in one archive.
build/libcore.a: build/core/.pin
	rm -rf build/coreobj && mkdir -p build/coreobj
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(CORE_INTEL_SRC) -c -o build/coreobj/abn_intel.o $(CB)/core/abn_intel.c
	for f in $$(sed -n 's/^CORE := //p' $(CB)/Makefile); do \
	    $(CC) $(CFLAGS) -I$(CB)/core -c -o build/coreobj/$$(basename $$f .c).o $(CB)/$$f || exit 1; done
	rm -f $@ && $(AR) rcs $@ build/coreobj/*.o

build/%_test: tests/%_test.c tests/check.h tests/coreeval.h $(CASIM_SRC) src/casim.h build/libcore.a
	$(CC) $(CFLAGS) -I$(CB)/core -Isrc -Itests -o $@ $< $(CASIM_SRC) $(CORE_LIBS)

# ---- elsewhere ----

# S3 code size of casim's own sources (the core's is measured in its own repo).
size:
	scripts/size-s3.sh

wasm:
	scripts/wasm.sh

clean:
	rm -rf build

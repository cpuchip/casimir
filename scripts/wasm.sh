#!/usr/bin/env bash
# The WebAssembly module and its wrapper, with emscripten through CMake (README.md), into build/wasm:
# casim.js, casim.wasm and casim.mjs; then the golden corpus through them under node. With emcc on PATH it is used;
# otherwise emscripten's own image, pinned by digest (emsdk 4.0.10), runs the build.
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE=emscripten/emsdk@sha256:90b757eb11fa9a0e3ce4d2d9f76d932a56018e4accc37b5a28b2783751e60eb7    # 4.0.10
TGZ=build/intel-dfp/IntelRDFPMathLib20U5.tar.gz
STEPS='set -e
mkdir -p build
emcmake cmake -S . -B build/wasm-cmake -DCMAKE_BUILD_TYPE=Release ${TARBALL:+-DCASIM_INTEL_TARBALL=$PWD/$TARBALL} >build/wasm-cmake.log
cmake --build build/wasm-cmake -j8 >>build/wasm-cmake.log 2>&1 || { tail -20 build/wasm-cmake.log; exit 1; }
mkdir -p build/wasm && cp build/wasm-cmake/casim.js build/wasm-cmake/casim.wasm build/wasm-cmake/casim.mjs build/wasm/
node tests/golden.mjs build/wasm tests/golden/corpus.txt
awk -F"\t" "BEGIN{OFS=\"\t\"} !d && !/^#/ && \$11==\"OK\" {\$12=\$12 \"1\"; d=1} {print}" tests/golden/corpus.txt > build/wasm/control.txt
if node tests/golden.mjs build/wasm build/wasm/control.txt >/dev/null 2>&1; then echo "wasm golden control: a changed answer passed" >&2; exit 1; fi
echo "wasm golden control: a changed answer is refused"'
TARBALL=""
[ -f "$TGZ" ] && TARBALL=$TGZ
if command -v emcc >/dev/null; then
    TARBALL=$TARBALL bash -c "$STEPS"
else
    docker run --rm --init --name "casim-wasm-$$" --memory 8g -v "$PWD":/w -w /w --user "$(id -u):$(id -g)" -e TARBALL="$TARBALL" -e HOME=/tmp "$IMAGE" bash -c "$STEPS"
fi
ls -l build/wasm/casim.wasm build/wasm/casim.js build/wasm/casim.mjs | awk '{print $5, $NF}'
printf 'gzip -9: %s bytes casim.wasm\n' "$(gzip -9 -c build/wasm/casim.wasm | wc -c | tr -d ' ')"

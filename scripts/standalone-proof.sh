#!/usr/bin/env bash
# The standalone proof (CAS 011 acceptance): a commit of this repository, and nothing else from this machine, builds
# the library, the command line, the C example and the WebAssembly wrapper in clean containers. The network is used
# for the distribution's packages and Intel's tarball only.
#
#   scripts/standalone-proof.sh [commit]     (default HEAD; the commit is exported with git archive)
set -euo pipefail
cd "$(dirname "$0")/.."
REV=${1:-HEAD}
UBUNTU=ubuntu@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3        # 24.04
EMSDK=emscripten/emsdk@sha256:90b757eb11fa9a0e3ce4d2d9f76d932a56018e4accc37b5a28b2783751e60eb7  # 4.0.10
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
git archive --format=tar "$REV" > "$T/casimir.tar"
echo "standalone proof of $(git rev-parse --short "$REV"), $(wc -c < "$T/casimir.tar") bytes of archive"
docker run --rm -v "$T/casimir.tar:/casimir.tar:ro" "$UBUNTU" bash -c '
    set -euo pipefail
    apt-get update -qq >/dev/null && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        build-essential curl ca-certificates cmake python3 >/dev/null
    mkdir /src && cd /src && tar -xf /casimir.tar
    ls /src/../abacus-firmware 2>/dev/null && { echo "a firmware checkout is visible: not a standalone proof"; exit 1; }
    make -s check
    build/hello
    cmake -S . -B /cm -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build /cm -j"$(nproc)" >/dev/null
    ctest --test-dir /cm 2>&1 | tail -3
    echo "C: library, command line, example, make check and ctest in a clean ubuntu 24.04"'
docker run --rm -v "$T/casimir.tar:/casimir.tar:ro" "$EMSDK" bash -c '
    set -euo pipefail
    mkdir /tmp/src && cd /tmp/src && tar -xf /casimir.tar
    scripts/wasm.sh
    echo "wasm: module, wrapper and the golden corpus under node in a clean emsdk 4.0.10"'

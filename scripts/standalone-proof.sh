#!/usr/bin/env bash
# The standalone proof (CAS 011 acceptance): a commit of this repository, and nothing else from this machine, builds
# the library, the command line, the C example and the WebAssembly wrapper in clean containers. The network is used
# for the distribution's packages and Intel's tarball only.
#
#   scripts/standalone-proof.sh [commit]     (default HEAD; the commit is exported with git archive)
#
# Each container is bounded: PROOF_MEMORY (default 4g), PROOF_CPUS (at most 8, from 0) and PROOF_TIMEOUT (1800 s).
# Any exit of this script, a time limit included, removes its containers by id.
set -euo pipefail
cd "$(dirname "$0")/.."
REV=${1:-HEAD}
UBUNTU=ubuntu@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3        # 24.04
EMSDK=emscripten/emsdk@sha256:90b757eb11fa9a0e3ce4d2d9f76d932a56018e4accc37b5a28b2783751e60eb7  # 4.0.10
MEM=${PROOF_MEMORY:-4g}         # each container's memory, swap included: a full box fails this run, not the box
N=$(docker info -f '{{.NCPU}}'); [ "$N" -gt 8 ] && N=8          # the daemon's CPUs (Docker Desktop's VM, not the host)
CPUS=${PROOF_CPUS:-0-$((N - 1))}  # nproc inside reads this, so the builds' -j (and their memory) is at most 8 anywhere
LIMIT=${PROOF_TIMEOUT:-1800}    # seconds for each container, enforced inside it too: it ends even if this script dies
T=$(mktemp -d)
stop() {                        # remove every container this run started, by the id docker wrote to its cidfile
    for f in "$T"/*.cid; do [ -s "$f" ] && docker rm -f "$(cat "$f")" >/dev/null 2>&1 || true; done
    rm -rf "$T"
}
trap stop EXIT
trap 'exit 130' INT TERM HUP
run() {                         # run NAME IMAGE SCRIPT: a bounded container, the archive on its stdin (no host path is
    local name=$1 image=$2 script=$3 rc=0   # mounted, so Docker Desktop's file sharing doesn't matter)
    timeout --kill-after=30 "$((LIMIT + 60))" docker run --rm -i --cidfile "$T/$name.cid" --memory "$MEM" \
        --memory-swap "$MEM" --cpuset-cpus "$CPUS" "$image" \
        timeout --kill-after=10 "$LIMIT" bash -c "$script" < "$T/casimir.tar" &
    wait $! || rc=$?            # in the background: a signal to this script runs its trap now, not when docker returns
    [ "$rc" = 0 ] || { echo "$name: failed with status $rc (124 is the time limit, 137 a kill: memory)"; exit 1; }
}
git archive --format=tar "$REV" > "$T/casimir.tar"
echo "standalone proof of $(git rev-parse --short "$REV"), $(wc -c < "$T/casimir.tar") bytes of archive"
run c "$UBUNTU" '
    set -euo pipefail
    apt-get update -qq >/dev/null
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        build-essential curl ca-certificates cmake python3 >/dev/null
    mkdir /src && cd /src && tar -xf - && exec < /dev/null
    ls /src/../abacus-firmware 2>/dev/null && { echo "a firmware checkout is visible: not a standalone proof"; exit 1; }
    make -s check
    build/hello
    cmake -S . -B /cm -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build /cm -j"$(nproc)" >/dev/null
    ctest --test-dir /cm 2>&1 | tail -3
    echo "C: library, command line, example, make check and ctest in a clean ubuntu 24.04"'
run wasm "$EMSDK" '
    set -euo pipefail
    mkdir /tmp/src && cd /tmp/src && tar -xf - && exec < /dev/null
    scripts/wasm.sh
    echo "wasm: module, wrapper and the golden corpus under node in a clean emsdk 4.0.10"'

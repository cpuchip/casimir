#!/usr/bin/env bash
# casim's sources under clang with the firmware's warning flags (-Wall -Wextra -Werror): the site's wasm build
# (emcc) is clang, and a warning only one compiler sees broke it once (primer #5429: -Wsign-compare at 3c6dc36,
# which gcc accepted). clang runs in a local image built once from Ubuntu's own clang; the planted control,
# tests/clang_control.c, must fail the same flags, or the check checks nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
# Pinned (abacus #5442), so a rebuilt image can't move the gate: the base by digest, the clang package by version,
# and the compiler's own --version checked on every run.
BASE=ubuntu@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3     # ubuntu:24.04, 2026-10-09
PKG="clang-18=1:18.1.3-1ubuntu1"
WANT="Ubuntu clang version 18.1.3 (1ubuntu1)"
IMAGE=casim-clang:18.1.3-1ubuntu1
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    printf 'FROM %s\nRUN apt-get update -qq && apt-get install -y -qq --no-install-recommends %s && ln -sf /usr/bin/clang-18 /usr/local/bin/clang && rm -rf /var/lib/apt/lists/*\n' "$BASE" "$PKG" |
        docker build -q -t "$IMAGE" - >/dev/null
fi
docker run --rm -v "$PWD":/w -w /w --user "$(id -u):$(id -g)" -e WANT="$WANT" "$IMAGE" bash -c '
    set -e
    if [ "$(clang --version | head -1)" != "$WANT" ]; then
        echo "clang check: the image has $(clang --version | head -1), not $WANT: refusing" >&2; exit 1
    fi
    F="-std=c11 -O2 -Wall -Wextra -Werror"
    for f in src/*.c; do clang $F -Ithird_party/abn -Isrc -c -o /dev/null "$f"; done
    if clang $F -c -o /dev/null tests/clang_control.c 2>/dev/null; then
        echo "clang check: the control compiled, so the check checks nothing" >&2; exit 1
    fi
    echo "clang check: $(clang --version | head -1), src/*.c clean, control refused"'

#!/usr/bin/env bash
# casim's sources under clang with the firmware's warning flags (-Wall -Wextra -Werror): the site's wasm build
# (emcc) is clang, and a warning only one compiler sees broke it once (primer #5429: -Wsign-compare at 3c6dc36,
# which gcc accepted). clang runs in a local image built once from Ubuntu's own clang; the planted control,
# tests/clang_control.c, must fail the same flags, or the check checks nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE=casim-clang:24.04
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    printf 'FROM ubuntu:24.04\nRUN apt-get update -qq && apt-get install -y -qq clang && rm -rf /var/lib/apt/lists/*\n' |
        docker build -q -t "$IMAGE" - >/dev/null
fi
docker run --rm -v "$PWD":/w -w /w --user "$(id -u):$(id -g)" "$IMAGE" bash -c '
    set -e
    F="-std=c11 -O2 -Wall -Wextra -Werror"
    for f in src/*.c; do clang $F -Ibuild/core/core -Isrc -c -o /dev/null "$f"; done
    if clang $F -c -o /dev/null tests/clang_control.c 2>/dev/null; then
        echo "clang check: the control compiled, so the check checks nothing" >&2; exit 1
    fi
    echo "clang check: $(clang --version | head -1), src/*.c clean, control refused"'

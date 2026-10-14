#!/usr/bin/env bash
# The ESP32-S3 cost of casim's own sources: each src/*.c compiled for the S3 with the firmware's
# optimization (-O2, CONFIG_COMPILER_OPTIMIZATION_PERF in the core's sdkconfig.defaults) and
# ESP-IDF's section flags, under the frame guard (no frame over 1 KB, a planted control proving it), then
# the three largest frames and the size table. Runs in the espressif/idf:v5.4.2 container.
# What a feature costs is this table's difference across its commit.
set -euo pipefail
cd "$(dirname "$0")/.."
shopt -s nullglob
srcs=(src/*.c)
if [ ${#srcs[@]} -eq 0 ]; then
    echo "size-s3: no sources in src/ yet"
    exit 0
fi
docker run --rm --init --name "casim-size-s3-$$" --memory 4g -v "$PWD":/w -w /w --user "$(id -u):$(id -g)" espressif/idf:v5.4.2 bash -c '
    set -e
    mkdir -p build/s3
    F="-std=c11 -O2 -mlongcalls -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wframe-larger-than=1024"
    for f in src/*.c; do
        xtensa-esp32s3-elf-gcc $F -fstack-usage -Ithird_party/abn -Isrc -c -o build/s3/$(basename "$f" .c).o "$f"
    done
    # the frame guard (CAS 004 rule 3) must refuse its planted 2 KB control, or it checks nothing
    if xtensa-esp32s3-elf-gcc $F -c -o /dev/null tests/frame_control.c 2>/dev/null; then
        echo "frame guard: the 2 KB control compiled, so the guard checks nothing" >&2; exit 1
    fi
    echo "largest frames (bytes, S3):"
    cat build/s3/*.su | sort -t"$(printf "\t")" -k2 -n -r | head -3 | sed "s/^/  /"
    xtensa-esp32s3-elf-size -t build/s3/*.o' | sed -n '/^largest/,$p'

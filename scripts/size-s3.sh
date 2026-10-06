#!/usr/bin/env bash
# The ESP32-S3 cost of casim's own sources: each src/*.c compiled for the S3 with the firmware's
# optimization (-O2, CONFIG_COMPILER_OPTIMIZATION_PERF in the core's sdkconfig.defaults) and
# ESP-IDF's section flags, then the size table. Runs in the espressif/idf:v5.4.2 container.
# What a feature costs is this table's difference across its commit.
set -euo pipefail
cd "$(dirname "$0")/.."
shopt -s nullglob
srcs=(src/*.c)
if [ ${#srcs[@]} -eq 0 ]; then
    echo "size-s3: no sources in src/ yet"
    exit 0
fi
docker run --rm -v "$PWD":/w -w /w --user "$(id -u):$(id -g)" espressif/idf:v5.4.2 bash -c '
    set -e
    mkdir -p build/s3
    for f in src/*.c; do
        xtensa-esp32s3-elf-gcc -std=c11 -O2 -mlongcalls -ffunction-sections -fdata-sections \
            -Wall -Wextra -Werror -Ibuild/core/core -Isrc -c -o build/s3/$(basename "$f" .c).o "$f"
    done
    xtensa-esp32s3-elf-size -t build/s3/*.o' | sed -n '/^ *text/,$p'

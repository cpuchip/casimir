#!/usr/bin/env bash
# CAS 016 rule 5: the developer tests in CI, on the public firmware at exactly CORE_PIN. The pin is fetched by its sha
# (depth 1); a pin the fetch can't find fails the job, and nothing is taken from a branch. `--ref` prints the commit it
# would fetch, so `make check` can hold it to CORE_PIN without the network.
set -euo pipefail
cd "$(dirname "$0")/.."
ref=$(cat CORE_PIN)
if [ "${1:-}" = --ref ]; then echo "$ref"; exit 0; fi
fw=${RUNNER_TEMP:-$PWD/build}/stu32-firmware
rm -rf "$fw" && git init -q "$fw"
git -C "$fw" fetch -q --depth 1 https://github.com/cpuchip/stu32-firmware.git "$ref"
git -C "$fw" cat-file -e "$ref^{commit}"
make test CORE_REPO="$fw"

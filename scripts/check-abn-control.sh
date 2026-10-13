#!/usr/bin/env bash
# check-abn's own control (CAS 011 acceptance): a copy of the vendored files with one byte of abn_intel.c changed
# must be refused by the SHA-256 check alone, and by the firmware comparison alone when a firmware clone is present.
set -euo pipefail
cd "$(dirname "$0")/.."
T=build/abn-control
rm -rf "$T" && mkdir -p build && cp -r third_party/abn "$T"
printf '\001' | dd of="$T/abn_intel.c" bs=1 seek=100 conv=notrunc 2>/dev/null
cmp -s "$T/abn_intel.c" third_party/abn/abn_intel.c && { echo "check-abn control: the byte was not planted" >&2; exit 1; }
modes="sha"
git -C "${CORE_REPO:-../stu32-firmware}" cat-file -e "$(cat CORE_PIN)^{commit}" 2>/dev/null && modes="sha git"
for m in $modes; do
    if ONLY=$m scripts/check-abn.sh "$T" >/dev/null 2>&1; then
        echo "check-abn control: ONLY=$m passed a planted byte, so it checks nothing" >&2; exit 1
    fi
done
rm -rf "$T"
echo "check-abn control: a planted byte in abn_intel.c refused by: $modes"

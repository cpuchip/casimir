#!/usr/bin/env bash
# The vendored number layer (third_party/abn) is the firmware's, unedited (CAS 011 rule 2). Two checks:
#   always:                  each file's SHA-256 is the one recorded in third_party/abn/PIN when it was vendored;
#   with a firmware clone:   each file is byte-identical to the firmware's at the recorded commit (CORE_REPO,
#                            default ../stu32-firmware; CI has only the first).
# The recorded commit must also be CORE_PIN, the core the developer tests use. Usage: scripts/check-abn.sh [dir]
# (dir defaults to third_party/abn; the planted-byte control in `make check-abn` passes a copy, with ONLY=sha or
# ONLY=git to show that each check alone refuses it).
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=${1:-third_party/abn}
PIN=third_party/abn/PIN
REPO=${CORE_REPO:-../stu32-firmware}
commit=$(sed -n 's/^commit //p' "$PIN")
if [ "$commit" != "$(cat CORE_PIN)" ]; then
    echo "check-abn: $PIN records $commit, but CORE_PIN is $(cat CORE_PIN)" >&2; exit 1
fi
fail=0
[ "${ONLY:-}" = git ] || while read -r sum name; do
    got=$(sha256sum "$DIR/$name" | cut -d' ' -f1)
    if [ "$got" != "$sum" ]; then echo "check-abn: $DIR/$name has SHA-256 $got, $PIN records $sum" >&2; fail=1; fi
done < <(grep -E '^[0-9a-f]{64}  ' "$PIN")
[ "$(grep -cE '^[0-9a-f]{64}  ' "$PIN")" -eq 2 ] || { echo "check-abn: $PIN should record 2 files" >&2; fail=1; }
mode=""
[ "${ONLY:-}" = git ] || mode="the recorded SHA-256s"
if [ "${ONLY:-}" != sha ] && git -C "$REPO" cat-file -e "$commit^{commit}" 2>/dev/null; then
    for name in abn.h abn_intel.c; do
        if ! git -C "$REPO" show "$commit:core/$name" | cmp -s - "$DIR/$name"; then
            echo "check-abn: $DIR/$name differs from the firmware's core/$name at $commit" >&2; fail=1
        fi
    done
    mode="${mode:+$mode and }the firmware at ${commit:0:7}"
fi
[ "$fail" -eq 0 ] || exit 1
[ -n "$mode" ] || { echo "check-abn: ONLY=git, but no firmware clone at $REPO" >&2; exit 1; }
echo "check-abn: $DIR matches $mode"

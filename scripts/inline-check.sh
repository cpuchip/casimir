#!/usr/bin/env bash
# derive's recursion must not gain a frame (work/steps-derive.md, oracle 5): deriv() is always inline, or each level
# of a derivative's recursion pays a call of its own (32 bytes on the S3, 46 levels: three times the stack's margin).
# The compiler's stack-usage report must hold no function deriv; a planted control (deriv made noinline) must hold
# one, or the check checks nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
CC=${CC:-cc}
D=build/inline
mkdir -p "$D"
su() {      # the stack-usage report's functions named deriv, for source $1
    "$CC" -std=c11 -O2 -fstack-usage -Ithird_party/abn -Isrc -c -o "$D/casim.o" "$1"
    awk -F'\t' '{n = split($1, a, ":"); if (a[n] == "deriv" || a[n] ~ /^deriv\./) print $1}' "$D/casim.su"
}
if [ -n "$(su src/casim.c)" ]; then
    echo "FAIL scripts/inline-check.sh: deriv has a frame of its own in src/casim.c (it must stay always_inline)"; exit 1
fi
sed 's/static inline __attribute__((always_inline)) ref deriv(/static __attribute__((noinline)) ref deriv(/' \
    src/casim.c > "$D/control.c"
if [ -z "$(su "$D/control.c")" ]; then
    echo "FAIL scripts/inline-check.sh: the noinline control shows no deriv frame, so the check checks nothing"; exit 1
fi
echo "inline check: deriv is inline in src/casim.c; control refused"

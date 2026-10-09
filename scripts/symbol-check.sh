#!/usr/bin/env bash
# casim must not pull a pow into the firmware (abacus #5589, after soroban #5586 measured Rev A at +49.5 KB: libm's double
# pow from int_root's guess, and libbid's __bid128_pow and its tables through abn_pow, which the firmware's own y^x
# never calls). Each src/*.c is compiled as the build compiles it and its undefined symbols are read with nm; any pow
# fails. The planted control, tests/symbol_control.c (it calls pow and abn_pow), must be caught, or the check checks
# nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
FORBID='^(pow|powf|powl|__ieee754_pow|abn_pow|__bid128_pow|bid128_pow)$'
tmp=$(mktemp -d build/symcheck.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
bad() { nm -u "$1" | awk '{print $NF}' | grep -E "$FORBID" || true; }
for f in src/*.c; do
    o="$tmp/$(basename "$f" .c).o"
    ${CC:-cc} -std=c11 -O2 -Ibuild/core/core -Isrc -c -o "$o" "$f"
    if [ -n "$(bad "$o")" ]; then
        echo "symbol check: $f needs $(bad "$o" | tr '\n' ' '): a pow the firmware would have to link" >&2
        exit 1
    fi
done
${CC:-cc} -std=c11 -O2 -Ibuild/core/core -Isrc -c -o "$tmp/control.o" tests/symbol_control.c
if [ -z "$(bad "$tmp/control.o")" ]; then
    echo "symbol check: the control's pow was not seen, so the check checks nothing" >&2
    exit 1
fi
echo "symbol check: src/*.c link no pow; control caught ($(bad "$tmp/control.o" | tr '\n' ' '))"

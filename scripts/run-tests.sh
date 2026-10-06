#!/usr/bin/env bash
# Runs each test binary given and reads its last line, "checks: N failed: F". A test passes only
# when it exits 0, made at least one check, and failed none: a test that checks nothing is
# refused, not passed.
set -u
fail=0
total=0
for t in "$@"; do
    out=$("$t" 2>&1)
    code=$?
    line=$(printf '%s\n' "$out" | grep -E '^checks: [0-9]+ failed: [0-9]+$' | tail -1)
    n=$(printf '%s' "$line" | sed -n 's/^checks: \([0-9]*\) failed: .*/\1/p')
    f=$(printf '%s' "$line" | sed -n 's/.* failed: \([0-9]*\)$/\1/p')
    if [ -z "$line" ] || [ "$n" -eq 0 ]; then
        printf '%s\n' "$out"
        echo "REFUSED $t: no checks line, or zero checks (exit $code)"
        fail=1
    elif [ "$code" -ne 0 ] || [ "$f" -ne 0 ]; then
        printf '%s\n' "$out"
        echo "FAIL $t: $f of $n checks failed (exit $code)"
        fail=1
    else
        echo "ok   $t: $n checks"
        total=$((total + n))
    fi
done
[ "$fail" -eq 0 ] && echo "all green: $# tests, $total checks"
exit "$fail"

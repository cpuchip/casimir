#!/usr/bin/env bash
# The golden test's own control: a copy of the corpus with one answer changed by one character must fail, or the
# test compares nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
T=build/golden-control.txt
awk -F'\t' 'BEGIN{OFS="\t"} !done && !/^#/ && $11=="OK" {$12=$12 "1"; done=1} {print}' tests/golden/corpus.txt > "$T"
if cmp -s "$T" tests/golden/corpus.txt; then echo "golden control: nothing was planted" >&2; exit 1; fi
if build/golden_check "$T" >/dev/null 2>&1; then echo "golden control: a changed answer passed" >&2; exit 1; fi
echo "golden control: a changed answer is refused"

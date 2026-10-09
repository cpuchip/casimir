#!/usr/bin/env bash
# The Intel Decimal Floating-Point Math Library, which abn_intel.c is written on: fetched from Intel's distribution
# at netlib (and nowhere else), its size and SHA-256 checked against the pin below, unpacked into
# build/intel-dfp/, and built with Intel's own makefile into build/intel/libbid.a.
#
#   scripts/fetch-intel-dfp.sh            fetch, verify, unpack, patch, build libbid.a
#   scripts/fetch-intel-dfp.sh --source   fetch, verify, unpack and patch only (the wasm build compiles the sources)
#
# The pin, the options and the const patch are the calculator firmware's (its scripts/fetch-intel-dfp.sh). The
# library is BSD-3-Clause; its notice is third_party/intel-dfp/LICENSE. The source is never committed: this script
# and its hash are the provenance. CMakeLists.txt fetches the same tarball against the same hash.
set -euo pipefail

URL="https://netlib.org/misc/intel/IntelRDFPMathLib20U5.tar.gz"
SIZE=5817987
SHA256="85dafd70f0fe2a8da218ade4233fca9d3228b0b04cd6d8527f7499926be01037"
# By-value arguments, rounding mode and status flags passed on every call (no globals), as abn_intel.c is built.
OPTS=(CALL_BY_REF=0 GLOBAL_RND=0 GLOBAL_FLAGS=0 UNCHANGED_BINARY_FLAGS=0)

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/build/intel-dfp"
TARBALL="$DEST/IntelRDFPMathLib20U5.tar.gz"
SRC="$DEST/IntelRDFPMathLib20U5"
OUT="$ROOT/build/intel"
CC="${CC:-cc}"

mkdir -p "$DEST" "$OUT"
if [ ! -f "$TARBALL" ]; then
    curl -fsSL -o "$TARBALL.part" "$URL"
    mv "$TARBALL.part" "$TARBALL"
fi
got_size=$(wc -c < "$TARBALL" | tr -d ' ')
got_sha=$(sha256sum "$TARBALL" 2>/dev/null | cut -d' ' -f1 || shasum -a 256 "$TARBALL" | cut -d' ' -f1)
if [ "$got_size" != "$SIZE" ] || [ "$got_sha" != "$SHA256" ]; then
    echo "fetch-intel-dfp: $TARBALL does not match the pin" >&2
    echo "  size   $got_size (want $SIZE)" >&2
    echo "  sha256 $got_sha (want $SHA256)" >&2
    exit 1
fi
[ -d "$SRC" ] || tar -xzf "$TARBALL" -C "$DEST"
# The trig functions' argument-reduction table (295 KB, one copy each in sin, cos and tan) is only read, but is
# declared without const, so a microcontroller build would place it in RAM. Made const so it goes to flash; any
# write to it would now fail to compile. Idempotent.
for f in sin cos tan; do
    sed -i.bak 's/^static BID_UINT384 bid_decimal128_moduli\[\] =/static const BID_UINT384 bid_decimal128_moduli[] =/' \
        "$SRC/LIBRARY/src/bid128_$f.c" && rm -f "$SRC/LIBRARY/src/bid128_$f.c.bak"
    grep -q '^static const BID_UINT384 bid_decimal128_moduli\[\] =' "$SRC/LIBRARY/src/bid128_$f.c" || {
        echo "fetch-intel-dfp: bid128_$f.c: moduli table not found for the const patch" >&2; exit 1; }
done
if [ "${1:-}" = "--source" ]; then
    echo "fetch-intel-dfp: verified $SHA256, sources in ${SRC#"$ROOT"/}"
    exit 0
fi
# The library's makefile does not track flags: a build with other flags starts clean.
STAMP="$SRC/.casim-flags"
FLAGS="$CC ${OPTS[*]}"
if [ "$(cat "$STAMP" 2>/dev/null)" != "$FLAGS" ]; then
    make -s -C "$SRC/LIBRARY" clean >/dev/null 2>&1 || true
    printf '%s' "$FLAGS" > "$STAMP"
fi
make -s -C "$SRC/LIBRARY" -j"$(nproc 2>/dev/null || echo 4)" CC="$CC" "${OPTS[@]}" >/dev/null
cp "$SRC/LIBRARY/libbid.a" "$OUT/libbid.a"
echo "fetch-intel-dfp: verified $SHA256, built ${OUT#"$ROOT"/}/libbid.a"

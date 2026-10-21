#!/usr/bin/env python3
"""CAS 020's oracles over the golden corpus (in make check), with CASIM_COMPLEX OR'd into each line's angle:
- the boundary: every line that isn't solve's, and every solve line in model 33, prints its recorded status and answer
  byte for byte (only solve reads the bit, and not in 33);
- rule 6: every other solve line prints the same with →DEC added (a root's text never goes through the decimal
  printer), status included;
- a control first: the solve lines with the bit must differ from the golden somewhere, or the bit does nothing and the
  two checks above check nothing."""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "golden_check")   # or CAS 022's check build
PRINT_DECIMAL, COMPLEX = 16, 32     # casim.h's CASIM_PRINT_DECIMAL and CASIM_COMPLEX


def lines():
    for line in open(os.path.join(ROOT, "tests", "golden", "corpus.txt"), encoding="utf-8"):
        if not line.startswith("#") and line.strip():
            yield line.rstrip("\n").split("\t")


def with_bits(rows, bits, inputs=False):
    """rows with bits OR'd into the angle; with inputs, only the ten input fields (golden_check --record reads those)"""
    out = []
    for f in rows:
        f = list(f[:10] if inputs else f)
        f[3] = str(int(f[3]) | bits)
        out.append("\t".join(f) + "\n")
    return out


def golden(rows, record=False):
    """golden_check over rows: (exit code, its output); with record, the recorded status and answer of each row"""
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as t:
        t.write("".join(rows))
    try:
        p = subprocess.run([GOLDEN] + (["--record"] if record else []) + [t.name], capture_output=True, text=True)
    finally:
        os.unlink(t.name)
    if record:
        return p.returncode, [l.split("\t")[10:12] for l in p.stdout.split("\n") if l and not l.startswith("#")]
    return p.returncode, (p.stdout.strip().split("\n")[-1:] or [""])[0], p.stderr


def main():
    all_rows = list(lines())
    solve = [f for f in all_rows if f[0] == "solve" and f[1] != "33"]
    rest = [f for f in all_rows if not (f[0] == "solve" and f[1] != "33")]
    ok = True
    rc, _, _ = golden(with_bits(solve, COMPLEX))
    if rc == 0 or not solve:
        print("FAIL scripts/complex-golden.py: with the bit every solve line matched the golden; the oracle checks nothing")
        sys.exit(1)
    rc, last, err = golden(with_bits(rest, COMPLEX))
    print(f"complex golden (CAS 020): {len(rest)} lines that aren't solve's or are model 33's, with CASIM_COMPLEX: {last}")
    if rc != 0:
        print(err.strip()[:2000])
        ok = False
    _, a = golden(with_bits(solve, COMPLEX, True), record=True)
    _, b = golden(with_bits(solve, COMPLEX | PRINT_DECIMAL, True), record=True)
    differ = [(f, x, y) for f, x, y in zip(solve, a, b) if x != y]
    print(f"complex golden: {len(solve)} solve lines ({len(a)} and {len(b)} recorded), CASIM_COMPLEX against CASIM_COMPLEX "
          f"with →DEC: {len(differ)} differ")
    for f, x, y in differ[:10]:
        print(f"  FAIL solve {f[7]}: {x} | with →DEC: {y}")
    ok = ok and not differ and len(a) == len(solve) == len(b)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

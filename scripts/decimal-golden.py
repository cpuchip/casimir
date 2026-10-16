#!/usr/bin/env python3
"""→DEC's oracle (CAS 019 rule 6, in make check): every line of tests/golden/decimal.txt (the corpus's answers at
4f24e3e, by CAS 002's rule) run with CASIM_PRINT_DECIMAL OR'd into its angle must print its recorded status and answer
byte for byte. A control first: the same run without the bit must fail on some line, or the check checks nothing."""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "build", "golden_check")
PRINT_DECIMAL = 16      # casim.h's CASIM_PRINT_DECIMAL


def run(bit):
    lines = []
    for line in open(os.path.join(ROOT, "tests", "golden", "decimal.txt"), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")
        f[3] = str(int(f[3]) | bit)
        lines.append("\t".join(f) + "\n")
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as t:
        t.write("".join(lines))
    try:
        p = subprocess.run([GOLDEN, t.name], capture_output=True, text=True)
    finally:
        os.unlink(t.name)
    return p.returncode, p.stdout.strip().split("\n")[-1], p.stderr


def main():
    rc, _, _ = run(0)
    if rc == 0:
        print("FAIL scripts/decimal-golden.py: without the bit every line matched; the oracle checks nothing")
        sys.exit(1)
    rc, last, err = run(PRINT_DECIMAL)
    print("decimal golden (→DEC): tests/golden/decimal.txt with CASIM_PRINT_DECIMAL; the control without it refused")
    print(last)
    if rc != 0:
        print(err.strip()[:2000])
    sys.exit(rc)


if __name__ == "__main__":
    main()

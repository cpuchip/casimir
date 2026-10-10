#!/usr/bin/env python3
"""docs/language.md's examples, run again (make check): each table row whose first cell is a `casim ...` command must
print exactly its second cell through build/casim (the answer, or the status and what it reports, one line; several
lines would be joined by " / "). A doc that drifts from the library fails here."""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs", "language.md")
CLI = os.path.join(ROOT, "build", "casim")
ROW = re.compile(r"^\| `casim ([^`]+)` \| `([^`]*)` \|$")


def main():
    n = failed = 0
    for i, line in enumerate(open(DOC, encoding="utf-8"), 1):
        m = ROW.match(line.rstrip("\n"))
        if not m:
            continue
        n += 1
        p = subprocess.run([CLI] + shlex.split(m.group(1)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8")
        got = " / ".join(p.stdout.strip().split("\n"))
        if got != m.group(2):
            failed += 1
            print(f"FAIL docs/language.md:{i}: casim {m.group(1)}\n  doc   {m.group(2)}\n  casim {got}")
    print(f"doc examples: {n} commands in docs/language.md\nchecks: {n} failed: {failed}")
    sys.exit(1 if failed or n == 0 else 0)


if __name__ == "__main__":
    main()

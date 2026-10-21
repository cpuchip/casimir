#!/usr/bin/env python3
"""docs/language.md's examples, work/013-steps-examples.txt's walks and work/016-examples.txt's rows, run again (make check): each table row whose
first cell is a `casim ...` command must print exactly its second cell through build/casim (the answer, or the status
and what it reports, one line; several lines would be joined by " / "). A doc that drifts from the library fails
here."""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = [os.path.join(ROOT, "docs", "language.md"), os.path.join(ROOT, "work", "013-steps-examples.txt"),
        os.path.join(ROOT, "work", "016-examples.txt"), os.path.join(ROOT, "work", "017-examples.txt"),
        os.path.join(ROOT, "work", "017-steps-examples.txt"), os.path.join(ROOT, "work", "018-examples.txt"),
        os.path.join(ROOT, "work", "019-examples.txt"), os.path.join(ROOT, "work", "020-examples.txt")]
CLI = os.path.join(ROOT, "build", "casim")
ROW = re.compile(r"^\| `casim ([^`]+)` \| `([^`]*)` \|$")


def main():
    n = failed = 0
    rows = [(os.path.relpath(d, ROOT), i, line) for d in DOCS for i, line in enumerate(open(d, encoding="utf-8"), 1)]
    for doc, i, line in rows:
        m = ROW.match(line.rstrip("\n"))
        if not m:
            if line.startswith("| `casim"):     # a row the pattern skips would never be run
                failed += 1
                print(f"FAIL {doc}:{i}: a casim row not in the form | `casim ...` | `answer` |")
            continue
        n += 1
        p = subprocess.run([CLI] + shlex.split(m.group(1)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8")
        got = " / ".join(p.stdout.strip().split("\n"))
        if got != m.group(2):
            failed += 1
            print(f"FAIL {doc}:{i}: casim {m.group(1)}\n  doc   {m.group(2)}\n  casim {got}")
    print(f"doc examples: {n} commands in {', '.join(os.path.relpath(d, ROOT) for d in DOCS)}\n"
          f"checks: {n} failed: {failed}")
    sys.exit(1 if failed or n == 0 else 0)


if __name__ == "__main__":
    main()

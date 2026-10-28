#!/usr/bin/env python3
"""docs/language.md's examples, work/013-steps-examples.txt's walks and work/016-examples.txt's rows, run again (make check): each table row whose
first cell is a `casim ...` command must print exactly its second cell through build/casim (the answer, or the status
and what it reports, one line; several lines would be joined by " / "). A doc that drifts from the library fails
here. CAS 021's boundary (rule 1): each row again through build/casim-ser0, the series route compiled out, must print
the same, except the rows tests/golden/moved/cas-021-examples.txt lists, which must print its text there (each from
NO_ROOM, UNDECIDED or DECIMAL_FORM), so today's routes are checked where the series route would answer in their place.
docs/language.md's prose (abacus #11634: 1.4.0 kept 1.3.0's line for a sum whose table row had moved): each backticked
answer in it must be in one of those rows or the golden corpus, or be listed in tests/doc-prose.txt as notation."""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = [os.path.join(ROOT, "docs", "language.md"), os.path.join(ROOT, "work", "013-steps-examples.txt"),
        os.path.join(ROOT, "work", "016-examples.txt"), os.path.join(ROOT, "work", "017-examples.txt"),
        os.path.join(ROOT, "work", "017-steps-examples.txt"), os.path.join(ROOT, "work", "018-examples.txt"),
        os.path.join(ROOT, "work", "019-examples.txt"), os.path.join(ROOT, "work", "020-examples.txt"),
        os.path.join(ROOT, "work", "022-examples.txt"), os.path.join(ROOT, "work", "023-examples.txt"),
        os.path.join(ROOT, "work", "024-examples.txt"), os.path.join(ROOT, "work", "025-examples.txt"),
        os.path.join(ROOT, "work", "026-examples.txt"), os.path.join(ROOT, "work", "027-examples.txt"),
        os.path.join(ROOT, "work", "028-examples.txt"), os.path.join(ROOT, "work", "029-examples.txt"),
        os.path.join(ROOT, "work", "030-examples.txt"), os.path.join(ROOT, "work", "031-examples.txt"),
        os.path.join(ROOT, "work", "032-examples.txt"), os.path.join(ROOT, "work", "033-examples.txt"),
        os.path.join(ROOT, "work", "034-examples.txt"), os.path.join(ROOT, "work", "035-examples.txt"),
        os.path.join(ROOT, "work", "036-examples.txt")]
CLI = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "casim")   # or CAS 022's check build
OFF = os.path.join(ROOT, "build", "casim-ser0")
MOVED = os.path.join(ROOT, "tests", "golden", "moved", "cas-021-examples.txt")
FROM = ("NO_ROOM", "UNDECIDED", "DECIMAL_FORM")
ROW = re.compile(r"^\| `casim ([^`]+)` \| `([^`]*)` \|$")
PROSE = os.path.join(ROOT, "tests", "doc-prose.txt")
CORPUS = os.path.join(ROOT, "tests", "golden", "corpus.txt")
ANSWER = re.compile(r"[÷×^≠<>≤≥]|^-?\d|SQRT|UNDEF|IMPROPER|TOO_LONG|NO_")      # backticked text that reads as an answer


def moved():
    """(doc, command) -> what it prints with the series route compiled out"""
    lines = [ln for ln in open(MOVED, encoding="utf-8").read().split("\n") if ln and not ln.startswith("#")]
    mv = {}
    for key, off in zip(lines[0::2], lines[1::2]):
        if not off.startswith("  off\t"):
            sys.exit(f"FAIL scripts/doc-examples.py: {MOVED} is not a row, then its text off, at {key}")
        mv[tuple(key.split("\t", 1))] = off.split("\t", 1)[1]
    return mv


def casim(cli, args):
    p = subprocess.run([cli] + shlex.split(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                       encoding="utf-8")
    return " / ".join(p.stdout.strip().split("\n"))


def prose(answers):
    """language.md's prose, a paragraph at a time (a backticked line may wrap): (answers checked, failures)"""
    notation = {ln for ln in open(PROSE, encoding="utf-8").read().split("\n") if ln and not ln.startswith("#")}
    for ln in open(CORPUS, encoding="utf-8"):
        f = ln.rstrip("\n").split("\t")
        if len(f) >= 12 and not ln.startswith("#"):
            answers.add(f[11])
    paras, cur = [], []
    for i, line in enumerate(open(DOCS[0], encoding="utf-8").read().split("\n"), 1):
        if not line.strip() or line.startswith("|"):
            cur = []
            continue
        if not cur:
            paras.append((i, cur))
        cur.append(line.strip())
    n = failed = 0
    seen = set()
    for i, lines in paras:
        text = " ".join(lines)
        for m in re.finditer(r"`([^`]+)`", text):
            t = m.group(1)
            at = i + sum(1 for k in range(1, len(lines)) if len(" ".join(lines[:k])) < m.start())   # the line it starts on
            if t.startswith("casim ") or not ANSWER.search(t):
                continue
            n += 1
            if t in notation:
                seen.add(t)
            elif not any(t in a for a in answers):
                failed += 1
                print(f"FAIL docs/language.md:{at}: `{t}` is in no checked row (a line the library moved?)")
    for t in sorted(notation - seen):
        failed += 1
        print(f"FAIL {os.path.relpath(PROSE, ROOT)} lists `{t}`, which docs/language.md's prose doesn't hold")
    return n, failed


def main():
    n = failed = 0
    mv, hit = moved(), set()
    rows = [(os.path.relpath(d, ROOT), i, line) for d in DOCS for i, line in enumerate(open(d, encoding="utf-8"), 1)]
    for doc, i, line in rows:
        m = ROW.match(line.rstrip("\n"))
        if not m:
            if line.startswith("| `casim"):     # a row the pattern skips would never be run
                failed += 1
                print(f"FAIL {doc}:{i}: a casim row not in the form | `casim ...` | `answer` |")
            continue
        n += 1
        got = casim(CLI, m.group(1))
        if got != m.group(2):
            failed += 1
            print(f"FAIL {doc}:{i}: casim {m.group(1)}\n  doc   {m.group(2)}\n  casim {got}")
        key = (doc, m.group(1))
        want = mv.get(key, m.group(2))
        if key in mv:
            hit.add(key)
            if want.split(" / ")[0] not in FROM:
                failed += 1
                print(f"FAIL {doc}:{i}: moved from {want.split(' / ')[0]}, which rule 1 never moves")
        off = casim(OFF, m.group(1))
        if off != want:
            failed += 1
            print(f"FAIL {doc}:{i}: casim {m.group(1)}, the series route compiled out\n  want  {want}\n  casim {off}")
    for key in set(mv) - hit:
        failed += 1
        print(f"FAIL {MOVED} lists a row no doc has: {key[0]}: casim {key[1]}")
    pn, pf = prose({ROW.match(line.rstrip("\n")).group(2) for _, _, line in rows if ROW.match(line.rstrip("\n"))})
    failed += pf
    print(f"doc examples: {n} commands in {', '.join(os.path.relpath(d, ROOT) for d in DOCS)}, each with the series "
          f"route compiled out too ({len(mv)} moved); {pn} answers in its prose\n"
          f"checks: {2 * n + len(mv) + pn} failed: {failed}")
    sys.exit(1 if failed or n == 0 else 0)


if __name__ == "__main__":
    main()

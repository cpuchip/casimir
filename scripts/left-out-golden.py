#!/usr/bin/env python3
"""CAS 028's boundary (make check; work/028-complex-left-out.md). The unit moves the golden lines
tests/golden/moved/cas-028.txt lists, and no others: with each listed line put back at its before, the corpus and
→DEC's file are c96b328's byte for byte (the sha256s in the list's header), each listed line's after is the file's,
and every after is its before with ", n CPLX" right after the status. n is judged apart from the code that prints it:
the same input solved with CASIM_COMPLEX (model 33 read as 35, which ignores the bit) counts n more roots, with their
multiplicities, than the before. Every solve line not listed that is SOLVED or PARTIAL has n = 0, or a line that the
note would take past 191 B (abacus #10148). The check's control: a before list with one line left at its after must
fail the hash. Later units' moves (LATER: CAS 032's, 035's and 036's) are put back first.
    scripts/left-out-golden.py [CLI]"""
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-028.txt")
CLI = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "casim")
LATER = [os.path.join(GOLDEN, "moved", "cas-032.txt"), os.path.join(GOLDEN, "moved", "cas-035.txt"),
         os.path.join(GOLDEN, "moved", "cas-036.txt")]
BASE = "c96b328"
MODEL = {"1": "STU", "33": "35", "35": "35"}
bad = 0


def fail(msg):
    global bad
    bad += 1
    print("FAIL scripts/left-out-golden.py: " + msg)


def moved(path=MOVED):
    """the list's sections (the corpus's moves, →DEC's): inputs (the first ten fields) -> (before, after), each
    [status, text]; and the header's sha256 for each file"""
    sections, lines, sha = [{}], [], {}
    for ln in open(path, encoding="utf-8").read().split("\n"):
        if ln.startswith("# " + BASE + " ") and " sha256 " in ln:
            name, _, h = ln[len("# " + BASE + " "):].split(" ")
            sha[name] = h
        elif ln.startswith("# Under →DEC"):
            sections.append({})
        elif ln and not ln.startswith("#"):
            lines.append((len(sections) - 1, ln))
    for i in range(0, len(lines), 3):
        (s, inp), (_, b), (_, a) = lines[i:i + 3]
        if not b.startswith("  before\t") or not a.startswith("  after\t"):
            sys.exit(f"FAIL scripts/left-out-golden.py: {MOVED} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections, sha


def count(text):
    """the roots in a solve answer, with their multiplicities"""
    if ": " not in text:
        return 0
    return sum(int(m.group(1)) if (m := re.search(r" \(MULT (\d+)\)$", r)) else 1
               for r in text.split(": ", 1)[1].split("; "))


def with_bit(f):
    """the same input's answer with CASIM_COMPLEX, or None where it isn't SOLVED or PARTIAL"""
    out = subprocess.run([CLI, "--arena", "65536", "--model", MODEL[f[1]], "--complex", "solve", f[7], chr(65 + int(f[2])),
                          "RAD"], stdout=subprocess.PIPE, text=True).stdout.strip()
    return out if out.startswith(("SOLVED", "PARTIAL")) else None


def noted(b, n):
    """b's text with ", n CPLX" after its status"""
    st, sep, rest = b.partition(": ")
    return f"{st}, {n} CPLX{sep}{rest}"


def before(name, mv, keep=None):
    """the file with mv's lines at their before (keep: one input left at its after), each line judged: its sha256"""
    out, hit = [], set()
    later = {}
    for path in LATER:                                  # a later unit's moves, at their before first
        (c_mv, d_mv), _ = moved(path)
        later.update(c_mv if name == "corpus.txt" else d_mv)
    for ln in open(os.path.join(GOLDEN, name), encoding="utf-8").read().split("\n"):
        f = ln.split("\t")
        key = "\t".join(f[:10])
        if ln and not ln.startswith("#") and key in later:
            f = f[:10] + later[key][0][:2]
        if ln and not ln.startswith("#") and key in mv:
            b, a = mv[key]
            hit.add(key)
            if keep is None:
                if f[10:12] != a[:2]:
                    fail(f"{name} has {f[10:12]} where cas-028.txt's after is {a}: {key}")
                on = with_bit(f)
                n = on and count(on) - count(b[1])
                if a[0] != b[0] or not n or a[1] != noted(b[1], n) or len(a[1].encode("utf-8")) > 191:
                    fail(f"{name}: its after isn't its before with \", n CPLX\" (n = {n} with the bit): {key}")
            if key != keep:
                f = f[:10] + b[:2]
        elif keep is None and ln and not ln.startswith("#") and f[0] == "solve" and f[10] == "OK" and \
                f[11].startswith(("SOLVED", "PARTIAL")):
            on = with_bit(f)
            n = on and count(on) - count(f[11])
            if n and len(noted(f[11], n).encode("utf-8")) <= 191:
                fail(f"{name}: a line left out of cas-028.txt that the note fits ({n} with the bit): {key}")
        out.append("\t".join(f))
    for key in set(mv) - hit:
        fail(f"cas-028.txt lists a line {name} doesn't have: {key}")
    return hashlib.sha256("\n".join(out).encode("utf-8")).hexdigest()


def main():
    (corpus_mv, dec_mv), sha = moved()
    for name, mv in (("corpus.txt", corpus_mv), ("decimal.txt", dec_mv)):
        if before(name, mv) != sha.get(name):
            fail(f"{name} with its moves put back isn't {BASE}'s")
    if before("corpus.txt", corpus_mv, keep=next(iter(corpus_mv))) == sha.get("corpus.txt"):
        fail("its control passed (a moved line left at its after)")
    print(f"left-out boundary: {len(corpus_mv)} moved in the corpus, {len(dec_mv)} under →DEC, each its before with "
          f"\", n CPLX\", n what the bit adds; every other solve line has none the note fits; with the moves put back, "
          f"both files are {BASE}'s; the control refused\n"
          f"checks: {2 * (len(corpus_mv) + len(dec_mv)) + 3} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

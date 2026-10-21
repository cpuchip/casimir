#!/usr/bin/env python3
"""CAS 022's boundary (make check; work/022-signs.md rule 4). The unit moves the golden lines
tests/golden/moved/cas-022.txt lists, and no others: with each listed line put back at its before, the corpus and
→DEC's file are CAS 021's b282bbe byte for byte (the sha256s in the list's header), each listed line's after is the
file's, and every after is a refusal (UNDECIDED). A later unit's moves (LATER: CAS 023's) are put back first. The check's
control: a before list with one line left at its after must fail the hash.
    scripts/signs-golden.py"""
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-022.txt")
TO = ("UNDECIDED",)
LATER = [os.path.join(GOLDEN, "moved", "cas-023.txt")]


def moved(path=MOVED):
    """the list's sections (the corpus's moves, →DEC's): inputs (the first ten fields) -> (before, after), each
    [status, text]; and the header's sha256 for each file"""
    sections, lines, sha = [{}], [], {}
    for ln in open(path, encoding="utf-8").read().split("\n"):
        if ln.startswith("# b282bbe ") and " sha256 " in ln:
            name, _, h = ln[len("# b282bbe "):].split(" ")
            sha[name] = h
        elif ln.startswith("# Under →DEC"):
            sections.append({})
        elif ln and not ln.startswith("#"):
            lines.append((len(sections) - 1, ln))
    for i in range(0, len(lines), 3):
        (s, inp), (_, b), (_, a) = lines[i:i + 3]
        if not b.startswith("  before\t") or not a.startswith("  after\t"):
            sys.exit(f"FAIL scripts/signs-golden.py: {MOVED} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections, sha


def before(name, mv, keep=None):
    """the file with mv's lines at their before (keep: one input left at its after): (failures, its sha256)"""
    bad, out, hit = 0, [], set()
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
            if f[10:12] != a[:2]:
                bad += 1
                print(f"FAIL scripts/signs-golden.py: {name} has {f[10:12]} where cas-022.txt's after is {a}: {key}")
            if a[0] not in TO:
                bad += 1
                print(f"FAIL scripts/signs-golden.py: {name}: moved to {a[0]}, not a refusal: {key}")
            if key != keep:
                f = f[:10] + b[:2]
        out.append("\t".join(f))
    for key in set(mv) - hit:
        bad += 1
        print(f"FAIL scripts/signs-golden.py: cas-022.txt lists a line {name} doesn't have: {key}")
    return bad, hashlib.sha256("\n".join(out).encode("utf-8")).hexdigest()


def main():
    (corpus_mv, dec_mv), sha = moved()
    bad = 0
    for name, mv in (("corpus.txt", corpus_mv), ("decimal.txt", dec_mv)):
        b, h = before(name, mv)
        bad += b
        if h != sha.get(name):
            bad += 1
            print(f"FAIL scripts/signs-golden.py: {name} with its moves put back isn't b282bbe's (sha256 {h})")
    _, h = before("corpus.txt", corpus_mv, keep=next(iter(corpus_mv)))
    if h == sha.get("corpus.txt"):
        bad += 1
        print("FAIL scripts/signs-golden.py: its control passed (a moved line left at its after)")
    print(f"signs boundary: {len(corpus_mv)} moved in the corpus, {len(dec_mv)} under →DEC, each to {', '.join(TO)}; "
          f"with them put back, both files are b282bbe's; the control refused\n"
          f"checks: {2 * (len(corpus_mv) + len(dec_mv)) + 3} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

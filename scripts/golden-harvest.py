#!/usr/bin/env python3
"""Aims the golden corpus where a fault is known to hide (abacus #5782): the developer tests' own inputs that kill a
mutant the corpus missed, added as inputs only, with answers recorded from the corpus's source (dbb6d4c). No new
oracle: still regression. Needs the developer tests' core (make test's build/libcore.a) and git.

    scripts/golden-harvest.py MUTANT...      prints, for each mutant, up to PER_MUTANT killing input lines

1. Each tests/*_test.c is built with tests/golden_log.c, which logs every call it makes to casim (-Wl,--wrap), and run:
   the pool is every distinct call, valid UTF-8, its arena and cap within golden_check's.
2. The pool is recorded through golden_check on dbb6d4c's src and on each mutant's (scripts/mutants.py's
   replacement), and a line whose status or text differs kills the mutant. The shortest are chosen.
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mutants  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = "dbb6d4c"                 # the corpus's source
PER_MUTANT = 3
ARENA_MAX, CAP_MAX = 1 << 19, 65536
CC = os.environ.get("CC", "cc")
CB = os.path.join(ROOT, "build", "core")
BID = ["-DDECIMAL_CALL_BY_REFERENCE=0", "-DDECIMAL_GLOBAL_ROUNDING=0", "-DDECIMAL_GLOBAL_EXCEPTION_FLAGS=0"]
WRAPS = ["derive", "simplify", "expand", "collect", "subst", "solve", "integrate", "factor", "run"]


def sh(cmd, **kw):
    subprocess.run(cmd, check=True, **kw)


def pool(work):
    log = os.path.join(work, "calls.log")
    wrap = ",".join(f"--wrap=casim_{w}" for w in WRAPS)
    for name in sorted(os.listdir(os.path.join(ROOT, "tests"))):
        if not name.endswith("_test.c"):
            continue
        exe = os.path.join(work, name[:-2])
        sh([CC, "-std=c11", "-O2", f"-I{CB}/core", "-Isrc", "-Itests", "-o", exe, f"tests/{name}", "src/casim.c",
            "tests/golden_log.c", f"-Wl,{wrap}", "build/libcore.a", f"{CB}/build/intel/libbid.a", "-lm"], cwd=ROOT)
        subprocess.run([exe], cwd=ROOT, env=dict(os.environ, GOLDEN_LOG=log), stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    seen, lines = set(), []
    for raw in open(log, "rb"):
        try:
            line = raw.decode("utf-8").rstrip("\n")
        except UnicodeDecodeError:
            continue                    # node reads the corpus as UTF-8 (tests/golden.mjs)
        f = line.split("\t")
        if any(ord(ch) < 32 and ch != "\t" for ch in line) or len(f) != 10:
            continue
        f[5] = str(min(int(f[5]), ARENA_MAX))   # casim uses at most 524,280 bytes: a larger arena replays the same
        if int(f[6]) > CAP_MAX:
            continue
        line = "\t".join(f)
        if line not in seen:
            seen.add(line)
            lines.append(line)
    return lines


def recorder(work, tag, src_text, hdr_text):
    d = os.path.join(work, tag)
    os.makedirs(os.path.join(d, "src"), exist_ok=True)
    open(os.path.join(d, "src", "casim.c"), "w", encoding="utf-8").write(src_text)
    open(os.path.join(d, "src", "casim.h"), "w", encoding="utf-8").write(hdr_text)
    exe = os.path.join(d, "golden_check")
    sh([CC, "-std=c11", "-O2", "-Ithird_party/abn", f"-I{d}/src", "-o", exe, "tests/golden_check.c",
        f"{d}/src/casim.c", "third_party/abn/abn_intel.c", *BID, "-isystem",
        "build/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src", "build/intel/libbid.a", "-lm"], cwd=ROOT)
    return exe


def record(exe, inputs):
    p = subprocess.run([exe, "--record", inputs], stdout=subprocess.PIPE, check=True, timeout=900)
    return p.stdout.decode("utf-8").rstrip("\n").split("\n")


def main(names):
    work = tempfile.mkdtemp(prefix="harvest-", dir=os.path.join(ROOT, "build"))
    lines = pool(work)
    inputs = os.path.join(work, "pool.txt")
    open(inputs, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    show = lambda path: subprocess.run(["git", "show", f"{REF}:{path}"], cwd=ROOT, stdout=subprocess.PIPE,
                                       check=True).stdout.decode("utf-8")
    want = record(recorder(work, "ref", show("src/casim.c"), show("src/casim.h")), inputs)
    print(f"# pool: {len(lines)} distinct calls from the developer tests", file=sys.stderr)
    cur = open(os.path.join(ROOT, "src", "casim.c"), encoding="utf-8").read()
    hdr = open(os.path.join(ROOT, "src", "casim.h"), encoding="utf-8").read()
    for name in names:
        text = cur
        for path, old, new in mutants.MUTANTS[name]:
            assert path == "src/casim.c" and text.count(old) == 1, f"{name}: not applicable"
            text = text.replace(old, new)
        try:
            got = record(recorder(work, name, text, hdr), inputs)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            print(f"# {name}: the mutant's recorder failed (a crash is a kill, but not a golden line)", file=sys.stderr)
            continue
        kills = [w for w, g in zip(want, got) if w != g]
        kills.sort(key=lambda w: (len(w.split("\t")[7]), w))
        print(f"# {name}: {len(kills)} of {len(want)} pool calls kill it", file=sys.stderr)
        if kills:
            print(f"# kills {name} (a developer test's input)")
        for w in kills[:PER_MUTANT]:
            print(w)


if __name__ == "__main__":
    main(sys.argv[1:])

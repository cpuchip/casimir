#!/usr/bin/env python3
"""The frame table (make check; abacus #6686): src/casim.c compiled as make check compiles it, with -fstack-usage, and
every function's frame compared with the table recorded for this compiler (tests/frames/<compiler>.su). A unit's rule
is that no existing function's frame moves: a frame that changes, or a function that disappears (inlined or merged
away), fails; a new function is listed, and recorded with --record when its unit is committed. A compiler with no
recorded table (another version, CI's) only records one, under build/frames, and says so: frames are the compiler's,
so only a table from the same compiler can judge them. The S3's own table stays the unit's manual check.

The control: the same source with CASIM_FRAME_PLANT set to a 200-byte local in an existing function must fail.

    scripts/frame-table.py              compare (and run the control)
    scripts/frame-table.py --record     write tests/frames/<compiler>.su from the source as it is"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.environ.get("CC", "cc")
CFLAGS = os.environ.get("CFLAGS", "-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror").split()
OUT = os.path.join(ROOT, "build", "frames")
SRC = os.environ.get("FRAME_SRC", os.path.join(ROOT, "src", "casim.c"))     # --record from another source (a unit's base)
PLANT = "-DCASIM_FRAME_PLANT=volatile char frame_plant_[200]; frame_plant_[0] = (char)op; if (frame_plant_[0] == 99) return CASIM_BAD_ARG;"


def compiler():
    try:
        v = subprocess.run([CC, "-dumpfullversion"], capture_output=True, text=True).stdout.strip()
    except FileNotFoundError:
        sys.exit(f"frame-table: no compiler {CC!r}")
    m = subprocess.run([CC, "-dumpmachine"], capture_output=True, text=True).stdout.strip()
    name = "clang" if "clang" in subprocess.run([CC, "--version"], capture_output=True, text=True).stdout else "gcc"
    return f"{name}-{v}-{m}"


def table(extra=()):
    """{function: bytes} for src/casim.c compiled with extra flags"""
    os.makedirs(OUT, exist_ok=True)
    obj = os.path.join(OUT, "casim.o")
    su = os.path.join(OUT, "casim.su")
    if os.path.exists(su):
        os.remove(su)
    p = subprocess.run([CC, *CFLAGS, "-fstack-usage", "-I" + os.path.join(ROOT, "third_party", "abn"),
                        "-I" + os.path.join(ROOT, "src"), *extra, "-c", "-o", obj, SRC],
                       capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit(f"frame-table: the compile failed:\n{p.stderr[-2000:]}")
    out = {}
    for line in open(su):
        f = line.rstrip("\n").split("\t")
        name = f[0].rsplit(":", 1)[-1]
        k = 2
        while name in out:                      # two statics of one name: the second is name#2, in the file's order
            name = f"{f[0].rsplit(':', 1)[-1]}#{k}"
            k += 1
        out[name] = int(f[1])
    return out


def read(path):
    t = {}
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        name, size = line.split("\t")
        t[name] = int(size)
    return t


def compare(base, now):
    moved = [f"{n}: {base[n]} -> {now[n]}" for n in sorted(base) if n in now and now[n] != base[n]]
    gone = [n for n in sorted(base) if n not in now]
    new = [f"{n}: {now[n]}" for n in sorted(now) if n not in base]
    return moved, gone, new


def main():
    tag = compiler()
    path = os.path.join(ROOT, "tests", "frames", tag + ".su")
    now = table()
    if "--record" in sys.argv:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(f"# casim's frames (bytes) from {tag}, -fstack-usage with make check's flags: scripts/frame-table.py\n")
            for n in sorted(now):
                f.write(f"{n}\t{now[n]}\n")
        print(f"frame-table: recorded {len(now)} frames for {tag}")
        return
    if not os.path.exists(path):
        with open(os.path.join(OUT, tag + ".su"), "w") as f:
            for n in sorted(now):
                f.write(f"{n}\t{now[n]}\n")
        print(f"frame-table: NOT CHECKED: no table recorded for {tag} (frames are the compiler's; the gate judges only on"
              f" the compiler it was recorded with); recorded build/frames/{tag}.su")
        return
    base = read(path)
    moved, gone, new = compare(base, now)
    # the control: a planted frame must be seen
    pm, pg, _ = compare(base, table([PLANT]))
    table()                                     # leave the real object behind
    fail = False
    if not pm and not pg:
        print("FAIL scripts/frame-table.py: the planted 200-byte frame was not seen; the gate checks nothing")
        fail = True
    for m in moved:
        print(f"FAIL frame moved: {m}")
    for g in gone:
        print(f"FAIL frame gone: {g}")
    if moved or gone or fail:
        sys.exit(1)
    print(f"frame-table: {len(base)} frames as recorded for {tag}"
          + (f"; new, not yet recorded: {', '.join(new)}" if new else "") + "; the planted frame was refused")


if __name__ == "__main__":
    main()

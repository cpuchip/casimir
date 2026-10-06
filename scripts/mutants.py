#!/usr/bin/env python3
"""Deliberate faults in casim, each of which the tests must catch.

Each mutant is one exact replacement in a source file, applied in a copy of the repo under
build/mutants/<name> (the build/ tree copied too, so the core is not rebuilt). A mutant counts only
when its text was found, it builds, and `make test` then fails; the tests that caught it are named.
Exit 0 only when every mutant is caught.

    scripts/mutants.py            every mutant
    scripts/mutants.py NAME ...   the named ones
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# name: (file, old text, new text). Unit CAS 001's four first, then ours.
MUTANTS = {
    # the angle factor dropped
    "angle-factor": ("src/casim.c",
                     "static node *trig(ctx *c, node *e)\n{\n    if (c->angle == CASIM_RAD) return e;",
                     "static node *trig(ctx *c, node *e)\n{\n    return e;"),
    # the chain rule's u' dropped: every function rule multiplies by 1 instead
    "chain-rule": ("src/casim.c",
                   "    if (is_int(du, 0)) return du;\n    switch (n->id) {",
                   "    if (is_int(du, 0)) return du;\n    du = mk_int(c, 1);\n    switch (n->id) {"),
    # the quotient rule's sign flipped
    "quotient-sign": ("src/casim.c",
                      "return s_div(c, s_sub(c, s_mul(c, da, b), s_mul(c, a, db)), fn(c, F_SQ, b));",
                      "return s_div(c, s_add(c, s_mul(c, da, b), s_mul(c, a, db)), fn(c, F_SQ, b));"),
    # 0×u folded to u
    "zero-times": ("src/casim.c",
                   "if (is_int(a, 0) || is_int(b, 0)) return mk_int(c, 0);",
                   "if (is_int(a, 0)) return b;\n    if (is_int(b, 0)) return a;"),
    # ours: u-u kept (the derivative of ASIN(X÷X) is then 0÷0 where the function is defined)
    "same-minus-same": ("src/casim.c",
                        "if (same(c, a, b)) return mk_int(c, 0);",
                        "if (same(c, a, b) && false) return mk_int(c, 0);"),
    # ours: a right operand of equal level printed bare (X÷(Y÷Z) as X÷Y÷Z)
    "printer-left-to-right": ("src/casim.c",
                              "print_in(c, w, b, level(b) <= lv);",
                              "print_in(c, w, b, level(b) < lv);"),
    # ours: a negated base printed bare ((-X)^2 as -X^2)
    "printer-pow-base": ("src/casim.c",
                         "print_in(c, w, a, level(a) < 4);",
                         "print_in(c, w, a, level(a) < 3);"),
}


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def main():
    names = sys.argv[1:] or list(MUTANTS)
    survived = 0
    for name in names:
        path, old, new = MUTANTS[name]
        tree = os.path.join(ROOT, "build", "mutants", name)
        shutil.rmtree(tree, ignore_errors=True)
        shutil.copytree(ROOT, tree, symlinks=True,
                        ignore=shutil.ignore_patterns(".git", "mutants", "s3"))
        src = os.path.join(tree, path)
        text = open(src, encoding="utf-8").read()
        if text.count(old) != 1:
            print(f"NOT APPLIED {name}: its text appears {text.count(old)} times in {path}")
            survived += 1
            continue
        open(src, "w", encoding="utf-8").write(text.replace(old, new))
        build = run(["make", "-s", "all"], tree)
        if build.returncode != 0:
            print(f"BUILD FAILED {name} (not counted):\n{build.stdout[-800:]}")
            survived += 1
            continue
        test = run(["make", "-s", "test"], tree)
        caught = [ln.split()[1] for ln in test.stdout.splitlines() if ln.startswith(("FAIL ", "REFUSED "))]
        if test.returncode != 0 and caught:
            print(f"caught   {name}: by {', '.join(c.rstrip(':') for c in caught)}")
            shutil.rmtree(tree, ignore_errors=True)
        else:
            print(f"SURVIVED {name}")
            survived += 1
    print(f"{len(names) - survived} of {len(names)} mutants caught")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())

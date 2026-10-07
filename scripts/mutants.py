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

# name: [(file, old text, new text), ...]: every replacement must apply. The unit CAS 002's four first,
# then CAS 001's four (rewritten for the canonical-form code), then ours.
MUTANTS = {
    # 003: expand not entering function arguments
    "expand-skips-fn-args": [
        ("src/casim.c", "        return fn(c, x->id, expand(c, x->a));", "        return r;"),
    ],
    # 003: collect printing one power twice (no merge after expanding)
    "collect-power-twice": [
        ("src/casim.c", "            while (j < nc && ct[j].k != k) j++;", "            j = nc;"),
    ],
    # 003: subst replacing only the first occurrence
    "subst-first-only": [
        ("src/casim.c", "        if (c->sub_val && x->id == c->sub_var) {",
         "        if (c->sub_val && x->id == c->sub_var && c->sub_count == 0) {"),
    ],
    # 003: RAND+RAND folded to 2×RAND (impurity ignored)
    "rand-folded": [
        ("src/casim.c", "    if (!c->has_impure || r == 0 || r == SPARE) return false;", "    if (1) return false;"),
    ],
    # 002: sorting skipped inside function arguments
    "sort-in-fn-args": [
        ("src/casim.c", "    case K_RFN: return fn(c, x->id, canon(c, x->a));",
         "    case K_RFN: { ref a_; nosort++; a_ = canon(c, x->a); nosort--; return fn(c, x->id, a_); }"),
        ("src/casim.c", "static void sort(ctx *c, ref *k, int n, int (*by)(ctx *, ref, ref))\n{\n",
         "static int nosort;\nstatic void sort(ctx *c, ref *k, int n, int (*by)(ctx *, ref, ref))\n{\n"
         "    if (nosort) return;\n"),
    ],
    # 002: a like-term collection that drops a coefficient
    "coefficient-dropped": [
        ("src/casim.c", "            if (x->kind == K_MUL && has_coef(c, list(c, x->a), x->n)) cq = *val(c, list(c, x->a)[0]);\n            ok = rat_add(&s2, &s2, &cq);",
         "            (void)x;\n            ok = rat_add(&s2, &s2, &cq);"),
    ],
    # 002: constant folding through a double
    "fold-through-double": [
        ("src/casim.c", "#include \"abn.h\"\n", "#include <stdio.h>\n#include \"abn.h\"\n"),
        ("src/casim.c",
         "static bool rat_add(rat *r, const rat *x, const rat *y)\n{\n",
         "static bool rat_from_abn(rat *r, const abn *x);\n"
         "static bool rat_add(rat *r, const rat *x, const rat *y)\n{\n"
         "    {\n"
         "    char to_[64];\n"
         "    abn v_;\n"
         "    snprintf(to_, 64, \"%.17g\", (double)x->p / (double)x->q + (double)y->p / (double)y->q);\n"
         "    if (abn_from_text(&v_, to_) == ABN_OK && rat_from_abn(r, &v_)) return true;\n"
         "    }\n"),
    ],
    # 002: an idempotence break: sorting before collecting, without a second pass
    "no-second-pass": [
        ("src/casim.c", "        m = k;\n        if (!again) break;", "        m = k;\n        (void)again;\n        break;"),
    ],
    # 001: the angle factor dropped
    "angle-factor": [
        ("src/casim.c", "    if (c->angle == CASIM_RAD) return small(c, C_1);\n    d = small",
         "    return small(c, C_1);\n    d = small"),
    ],
    # 001: the chain rule's u' dropped
    "chain-rule": [
        ("src/casim.c", "        return deriv_fn(c, r, du);", "        return deriv_fn(c, r, small(c, C_1));"),
    ],
    # 001: the quotient rule's sign flipped (a quotient is u×v^-1: the power rule's e carries its sign)
    "quotient-sign": [
        ("src/casim.c", "if (ce) return mul3(c, e, power(", "if (ce) return mul3(c, neg(c, e), power("),
    ],
    # 001: 0×u folded to u
    "zero-times": [
        ("src/casim.c", "            if (is_k(c, t[i], 0, 1))                                  /* 0×u is 0: but a draw stays */",
         "            if (is_k(c, t[i], 0, 1) && (t[i] = small(c, C_1), false))"),
    ],
    # 002b: the depth guard off (no limit: deep input then crashes the S3, and returns OK on the host)
    "depth-guard-off": [
        ("src/casim.c", "    if (c->depth >= CASIM_DEPTH_MAX) {", "    if (0) {"),
    ],
    # ours: u-u kept (a zero coefficient left in the sum)
    "u-minus-u-kept": [
        ("src/casim.c", "        if (rat_is(&s2, 0, 1)) continue;", "        if (0) continue;"),
    ],
    # ours: a power's base printed bare when it is a product ((2×X)^1.5 as 2×X^1.5)
    "printer-pow-base": [
        ("src/casim.c", "        print_in(c, w, b, level(c, b) < 4);\n        puts_(w, \"^\");\n        put_exp(c, w, x);",
         "        print_in(c, w, b, level(c, b) < 2);\n        puts_(w, \"^\");\n        put_exp(c, w, x);"),
    ],
}


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def main():
    names = sys.argv[1:] or list(MUTANTS)
    survived = 0
    for name in names:
        tree = os.path.join(ROOT, "build", "mutants", name)
        shutil.rmtree(tree, ignore_errors=True)
        shutil.copytree(ROOT, tree, symlinks=True,
                        ignore=shutil.ignore_patterns(".git", "mutants", "s3", "device"))
        applied = True
        for path, old, new in MUTANTS[name]:
            src = os.path.join(tree, path)
            text = open(src, encoding="utf-8").read()
            if text.count(old) != 1:
                print(f"NOT APPLIED {name}: its text appears {text.count(old)} times in {path}")
                applied = False
                break
            open(src, "w", encoding="utf-8").write(text.replace(old, new))
        if not applied:
            survived += 1
            continue
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

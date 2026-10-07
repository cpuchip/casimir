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
    # 003 review, delta passes: the order-free adder, sums made primitive that merge, collect's c_k
    "add-one-end": [
        ("src/casim.c", "term_coef(c, t[s.p > 0 ? lo++ : hi--])", "term_coef(c, t[lo++])"),
    ],
    "run-unsorted": [
        ("src/casim.c", "        sort(c, t + i, j - i, by_coef);", "        (void)by_coef;"),
    ],
    "one-denominator-group": [
        ("src/casim.c", "            int64_t d = term_coef(c, t[i]).q;", "            int64_t d = 1;"),
        ("src/casim.c", "            if (term_coef(c, t[lo]).q != q) lo++;\n            else if (term_coef(c, t[hi]).q != q) hi--;\n            else if",
         "            if"),
    ],
    "sum-div-unmerged": [
        ("src/casim.c", "    return c->apart ? add(c, list(c, l), n) : flat_node(c, K_ADD, l, n);", "    return flat_node(c, K_ADD, l, n);"),
    ],
    "collect-ck-raw": [
        ("src/casim.c", "if (nd(c, ct[i].c)->kind == K_ADD) ct[i].c = mul(c, &ct[i].c, 1);", "(void)ct;"),
    ],
    "apart-not-set-by-run": [
        ("src/casim.c", "        if (!ok) {\n            c->apart = true;", "        if (!ok) {"),
    ],
    "apart-not-set-by-fold": [
        ("src/casim.c", "    if (!ok) c->apart = true;\n", ""),
    ],
    "content-apart": [
        ("src/casim.c", "        for (int q = 0; f && q < nf; q++)\n            if (is_num(c, f[q])) {", "        for (int q = 0; f && q < nf; q++)\n            if (q < 0) {"),
    ],
    "no-walk-fallback": [
        ("src/casim.c", "    if (add_by_den(c, t, n, acc)) return true;\n    *acc = a0;", "    if (add_by_den(c, t, n, acc)) return true;\n    (void)a0;\n    return false;"),
    ],
    "no-resort": [
        ("src/casim.c", "        for (int q = 0; q < nf; q++) resort |= is_num(c, f[q]);\n", ""),
    ],
    "sign-from-first": [
        ("src/casim.c", "if (frac_cmp(a.p, a.q, big.p < 0 ? -big.p : big.p, big.q) > 0) big = v;", "if (big.p == 0) big = v;"),
    ],
    "no-sign-content": [
        ("src/casim.c", "                if (big.p < 0) rat_int(&k, -1);\n                return k;", "                (void)big;\n                return k;"),
    ],
    # CAS 004: the stamp ignored, STU's exponent minus as the 35s's, the replacement's stamp ignored,
    # TOO_LONG truncated instead of refused (the unit's list, casim's side)
    "stamp-ignored": [
        ("src/casim.c", "    rtl = rq->model == CASIM_MODEL_STU;", "    rtl = false;"),
    ],
    "stu-exponent-minus-tight": [
        ("src/casim.c", "    return c->rtl && op == O_PNEG ? LEVEL[O_NEG] : LEVEL[op];", "    return (void)c, LEVEL[op];"),
    ],
    "repl-stamp-ignored": [
        ("src/casim.c", "    c.rtl = repl_rtl;                   /* read as its own model (CAS 004) */", "    (void)repl_rtl;"),
    ],
    "too-long-truncated": [
        ("src/casim.c", "        if (w.full) c->st = CASIM_TOO_LONG;\n", ""),
        ("src/casim.c", "    if (w.full || c->st == CASIM_TOO_LONG || c->st == CASIM_NO_ROOM) out[0] = '\\0';",
         "    if (c->st == CASIM_TOO_LONG || c->st == CASIM_NO_ROOM) out[0] = '\\0';"),
    ],
    "no-room-as-too-long": [
        ("src/casim.c", "    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_NO_ROOM;     /* the arena */",
         "    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_TOO_LONG;"),
        ("src/casim.c", "        if (c->st == CASIM_OK) c->st = CASIM_NO_ROOM;          /* the stack's bound (CAS 004: not TOO_LONG) */",
         "        if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;"),
    ],
    "zero-half-level": [
        ("src/casim.c", "    return abs_is(c, x, 1, 2) && !(num_negative(c, x) && !below(c, f)) ? 5 : 4;",
         "    return abs_is(c, x, 1, 2) ? 5 : 4;"),
    ],
    # decision 52: a chain of powers printed bare
    "chain-bare": [
        ("src/casim.c", "    for (int i = 1; i < k; i++) puts_(w, \"(\");", "    (void)0;"),
        ("src/casim.c", "        if (i > 0) puts_(w, \")\");\n", ""),
    ],
    "chain-base-bare": [
        ("src/casim.c", "    print_in(c, w, b, level(c, b) < 5);                     /* a base with a ^ of its own: (X^2)^Y */",
         "    print_in(c, w, b, level(c, b) < 4);"),
    ],
    # 003: subst replacing only the first occurrence
    "subst-first-only": [
        ("src/casim.c", "    if (c->sub_val && nd(c, r)->id == c->sub_var) {",
         "    if (c->sub_val && nd(c, r)->id == c->sub_var && c->sub_count == 0) {"),
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
        ("src/casim.c", "    if (x->kind == K_MUL && has_coef(c, list(c, x->a), x->n)) k = *val(c, list(c, x->a)[0]);\n    return k;",
         "    (void)x;\n    return k;"),
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
        ("src/casim.c", "        print_in(c, w, b, level(c, b) < 5);                 /* (X^Y)^0.3 (decision 52) */\n        puts_(w, \"^\");",
         "        print_in(c, w, b, level(c, b) < 2);\n        puts_(w, \"^\");"),
    ],
}


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")


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

#!/usr/bin/env python3
"""Deliberate faults in casim, each of which the tests must catch.

Each mutant is one exact replacement in a source file, applied in a copy of the repo under
build/mutants/<name> (the build/ tree copied too, so the core is not rebuilt). A mutant counts only
when its text was found, it builds, and `make test` then fails; the tests that caught it are named.
Exit 0 only when every mutant is caught.

The frame table is re-recorded in each copy before its test (CAS 014): a fault often moves a frame, and the frame
gate runs first in make check, so it would stop the run before any test of values and "catch" a mutant only because
its code changed shape. The faults whose catch is the frame table itself are listed in FRAME_CAUGHT.

    scripts/mutants.py            every mutant
    scripts/mutants.py NAME ...   the named ones
    scripts/mutants.py --anchors  only that every anchor is found once (make check; CAS 023: a full run's
                                  shards stopped on five anchors the build had moved)
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
    # CAS 005: the unit's list
    "solve-root-dropped": [
        ("src/casim.c", "        if (x->deg == 1) rational_root(rs, &nr, x->b, x->a, x->mult);",
         "        if (x->deg == 1 && x->mult > 1) rational_root(rs, &nr, x->b, x->a, x->mult);"),
    ],
    "solve-multiplicity-lost": [
        ("src/casim.c", "    if (x->mult > 1) puts_(w, \" (MULT \"), put_int(w, x->mult), puts_(w, \")\");",
         "    if (x->mult > 9) puts_(w, \" (MULT \"), put_int(w, x->mult), puts_(w, \")\");"),
    ],
    "solve-verification-skipped": [
        ("src/casim.c", "    return !__builtin_mul_overflow(p, r, &t) && !__builtin_add_overflow(h->a[0], t, &t) && t == 0;",
         "    return !__builtin_mul_overflow(p, r, &t) && !__builtin_add_overflow(h->a[0], t, &t);"),
    ],
    "solve-partial-as-solved": [
        ("src/casim.c", "    if (f->n >= 3) {\n        puts_(w, \"PARTIAL, DEGREE \")", "    if (f->n >= 30) {\n        puts_(w, \"PARTIAL, DEGREE \")"),
    ],
    "solve-plus-minus-collapsed": [
        ("src/casim.c", "    for (int s = -1; s <= 1; s += 2) {\n        iroot x", "    for (int s = 1; s <= 1; s += 2) {\n        iroot x"),
    ],
    "solve-stamp-ignored": [
        ("src/casim.c", "    return (model == CASIM_MODEL_STU ? SF_RTL : 0) | ((angle & ANGLE_CX)",
         "    return (0 ? SF_RTL : 0) | ((angle & ANGLE_CX)"),
    ],
    # CAS 006: the unit's list, and each of the check's two rewrites (abacus #5124)
    "integ-substitution-a-dropped": [
        ("src/casim.c", "        return mul2(c, r, a_inv);\n    }", "        (void)a_inv;\n        return r;\n    }"),
    ],
    "integ-sin-sign-lost": [
        ("src/casim.c", "        case F_SIN: r = neg(c, mul2(c, fn(c, F_COS, uc), angle_factor(c, true))); break;",
         "        case F_SIN: r = mul2(c, fn(c, F_COS, uc), angle_factor(c, true)); break;"),
    ],
    "integ-abs-dropped": [
        ("src/casim.c", "    if (rat_is(n, -1, 1)) return mul2(c, fn(c, F_LN, fn(c, F_ABS, uc)), power(c, pnum(c, &lin[1]), small(c, C_M1)));",
         "    if (rat_is(n, -1, 1)) return mul2(c, fn(c, F_LN, uc), power(c, pnum(c, &lin[1]), small(c, C_M1)));"),
    ],
    "integ-deg-factor-lost": [
        ("src/casim.c", "        case F_COS: r = mul2(c, fn(c, F_SIN, uc), angle_factor(c, true)); break;",
         "        case F_COS: r = fn(c, F_SIN, uc); break;"),
    ],
    "integ-verification-skipped": [
        ("src/casim.c", "    t = expand_fully(c, t);\n    return c->st == CASIM_OK && is_k(c, t, 0, 1);\n}",
         "    t = expand_fully(c, t);\n    return c->st == CASIM_OK || is_k(c, t, 0, 1);\n}"),
        ("src/casim.c", "        case F_EXP: r = fn(c, F_EXP, uc); break;", "        case F_EXP: r = fn(c, F_EXP, mul2(c, uc, small(c, C_2))); break;"),
    ],
    "integ-stamp-ignored": [
        ("src/casim.c", "    st = do_integrate(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);",
         "    st = do_integrate(eqn, var, angle, out, cap, arena, arena_cap, ((void)model, false));"),
    ],
    "integ-plus-c": [
        ("src/casim.c", "    if (c.st == CASIM_OK) F = integ(&c, n, var);         /* each term checked as it goes (integ_checked) */",
         "    if (c.st == CASIM_OK) F = add2(&c, integ(&c, n, var), leaf(&c, K_VAR, 2));"),
    ],
    "integ-check-no-sgn-abs": [
        ("src/casim.c", "g->a) != 0) continue;\n                    list(c, l)[i] = power(c, g->a, small(c, C_M1));\n                    list(c, l)[j] = small(c, C_1);",
         "g->a) != 0) continue;\n                    (void)l;"),
    ],
    "integ-check-no-tan": [
        ("src/casim.c", "        if (x->id == F_TAN && !c->sub_val) v = mul2(c, fn(c, F_SIN, a), power(c, fn(c, F_COS, a), small(c, C_M1)));",
         "        if (0) v = a;"),
    ],
    # CAS 007: the unit's list, then the search's own parts (the disks' pairing, a wide interval, the finder)
    "factor-dropped": [
        ("src/casim.c", "                if (times && *ni <= SOLVE_DEG) {\n                    it[(*ni)++] = (fitem){g, times, false};",
         "                if (times && *ni <= SOLVE_DEG) {\n                    (void)it;"),
    ],
    "factor-multiplicity-lost": [
        ("src/casim.c", "            if (it[i].mult > 1) puts_(&o, \"^\"), put_int(&o, it[i].mult);", "            (void)0;"),
    ],
    "factor-content-lost": [
        ("src/casim.c", "!rat_is(&content, 1, 1)) put_content(&o, &content, kud ? 1 : 3);",
         "!rat_is(&content, 1, 1)) (void)put_content;"),
    ],
    "factor-content-sign-flipped": [
        ("src/casim.c", "        bool kud = content.q != 1 && !(c.nm & NM_DEC);",
         "        if (content.p < 0) content.p = -content.p;\n        bool kud = content.q != 1 && !(c.nm & NM_DEC);"),
    ],
    # CAS 019's k×u÷d content (abacus #8758): →DEC keeps p÷q×u, a lone sum is parenthesised before ÷q, and q is printed
    "factor-content-kud-under-dec": [
        ("src/casim.c", "        bool kud = content.q != 1 && !(c.nm & NM_DEC);", "        bool kud = content.q != 1;"),
    ],
    "factor-content-sum-bare": [
        ("src/casim.c", "(items > 1 || it[i].mult > 1 || o.n > 0 || kud);", "(items > 1 || it[i].mult > 1 || o.n > 0);"),
    ],
    "factor-content-divisor-dropped": [
        ("src/casim.c", "        if (kud) put_content(&o, &content, 2);", "        (void)kud;"),
    ],
    "factor-check-skipped-wrong-factor": [
        ("src/casim.c", "    return same;\n}", "    return same || 1;\n}"),
        ("src/casim.c", "    if (zero) it[ni++] = (fitem){{1, {0, 1}}, zero, true};", "    if (zero) it[ni++] = (fitem){{1, {1, 1}}, zero, true};"),
    ],
    "factor-unproven-reported-ok": [
        ("src/casim.c", "        partial = partial || r == 1;", "        partial = partial && r == 1;"),
    ],
    "factor-stamp-ignored": [
        ("src/casim.c", "    return do_factor(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);",
         "    return do_factor(eqn, var, angle, out, cap, arena, arena_cap, ((void)model, false));"),
    ],
    # (every root taken as real only widens the search, an equivalent mutant; pairing real roots hides a factor)
    "factor-real-roots-paired": [
        ("src/casim.c", "        if (count == 0) pair[i] = i;", "        if (count == 0) pair[i] = (i ^ 1) < n ? i ^ 1 : i;"),
    ],
    # (an interval 0.5 or wider taken as a rejection survives: since the constant-term window, no search in about
    # 344,000 probes, Eisenstein products, dense and real-rooted inputs with divisor-rich leads, left one unproven by
    # a wide interval. The branch stays, as the proof needs it; the mutant is equivalent on everything reached.)
    "factor-window-too-narrow": [
        ("src/casim.c", "            dmin = (1 - 1e-9) / hi, dmax = lo > 0 ? a0 * (1 + 1e-9) / lo : 1e300;",
         "            dmin = (1 - 1e-9) / hi, dmax = lo > 0 ? a0 * (1 + 1e-9) / lo / 2 : 1e300;"),
    ],
    "factor-finder-not-reapplied": [
        ("src/casim.c", "        repeated_factors(&it[j].p, s, w, dq, it, &ni);\n", ""),
    ],
    "factor-finder-off": [
        ("src/casim.c", "    repeated_factors(&f, s, w, dq, it, &ni);", "    (void)repeated_factors;"),
    ],
    # CAS 008: the unit's list
    "exact-table-sign-flipped": [
        ("src/casim.c", "{0, 0, 0, 0}, {1, 1, 3, 3}, {1, 1, 1, 1}", "{0, 0, 0, 0}, {-1, 1, 3, 3}, {1, 1, 1, 1}"),
    ],
    "exact-deg-read-as-rad": [
        ("src/casim.c", "    if (c->angle != CASIM_RAD) {\n        rat d;\n        if (!is_num(c, a)) return false;",
         "    if (c->angle == CASIM_GRAD) {\n        rat d;\n        if (!is_num(c, a)) return false;"),
    ],
    "exact-odd-twelfth-simplified": [
        ("src/casim.c", "{{0, 1, 1}, {0, 0, 0}, {1, 2, 1}", "{{0, 1, 1}, {1, 4, 1}, {1, 2, 1}"),
    ],
    "exact-imperfect-power-simplified": [
        ("src/casim.c", "        if (!over && v == n) return *r = t, true;", "        if (!over) return *r = t, true;"),
    ],
    "exact-inverse-branch-wrong": [
        ("src/casim.c", "lo = f == F_ACOS ? 0 : -6,", "lo = f == F_ACOS ? -12 : -6,"),
    ],
    "exact-undefined-not-raised": [
        ("src/casim.c", "            c->st = CASIM_UNDEFINED;            /* CAS 008 rule 5: an exact 0 in a denominator has no value */\n            return SPARE;",
         "            (void)0;"),
    ],
    # CAS 009: the unit's list
    "defint-endpoint-pole-allowed": [
        ("src/casim.c", "if (ex->p < 0 && abn_cmp(&umin, &tol) <= 0 && abn_cmp(&umax, &ntol) >= 0) return IV_IMPROPER;",
         "if (ex->p < 0 && abn_cmp(&umin, &ntol) < 0 && abn_cmp(&umax, &tol) > 0) return IV_IMPROPER;"),
    ],
    "defint-tan-poles-as-rad": [
        ("src/casim.c", "    if (c->angle == CASIM_RAD) {\n        abn two = abn_from_int(2);\n        per = abn_pi();",
         "    if (1) {\n        abn two = abn_from_int(2);\n        per = abn_pi();"),
    ],
    "defint-tan-poles-sparse": [         # abacus #5577: as-RAD is caught only by over-refusing; this places them too few
        ("src/casim.c", "per = abn_from_int(c->angle == CASIM_DEG ? 180 : 200);", "per = abn_from_int(c->angle == CASIM_DEG ? 360 : 400);"),
    ],
    "defint-lo-minus-hi": [
        ("src/casim.c", "    return finish(&c, add2(&c, b, neg(&c, a)), out, ocap);", "    return finish(&c, add2(&c, a, neg(&c, b)), out, ocap);"),
    ],
    "defint-fraction-edge-ignored": [
        ("src/casim.c", "    if (ex->q != 1 && abn_cmp(&umin, &z) < 0) return IV_IMPROPER;",
         "    if (ex->q != 1 && abn_cmp(&umin, &z) < 0 && 0) return IV_IMPROPER;"),
    ],
    "defint-abs-fold-sign": [
        ("src/casim.c", "            if (sg) return sg > 0 ? a : neg(c, a);", "            if (sg) return sg < 0 ? a : neg(c, a);"),
    ],
    "defint-check-walks-F": [
        ("src/casim.c", "interval_walk(&c, f, var, &ba, &bb) : interval_walk(&c, f, var, &bb, &ba);",
         "interval_walk(&c, ((void)f, F), var, &ba, &bb) : interval_walk(&c, F, var, &bb, &ba);"),
    ],
    # CAS 010: the unit's list
    "poly-degree-not-minimized": [
        ("src/casim.c", "        if (nh > best) continue;                        /* an unlucky prime: its gcd's degree is too large */",
         "        if (nh > best && 0) continue;                   /* an unlucky prime: its gcd's degree is too large */"),
        ("src/casim.c", "        if (nh < best) best = nh, have = false;         /* a smaller degree: start the reconstruction again */",
         "        if (nh != best) best = nh, have = false;        /* a smaller degree: start the reconstruction again */"),
    ],
    "poly-candidate-test-skipped": [
        ("src/casim.c", "        if (div_poly(f, cand, q) && div_poly(g, cand, q)) return *out = *cand, true;",
         "        if (1) return (void)q, *out = *cand, true;"),
    ],
    # (poly-garner-overflow-unchecked, the unit's list, is equivalent within rule 1's bounds: a coefficient can't
    # overflow while M×p fits, since |acc'| <= M/2 + M×p/2; and M×p only overflows on a third combined 31-bit prime,
    # while the candidate is at most gcd(lc f, lc g)/lc(G) × the factor bound, about 3.2E17, which two lucky primes
    # (M about 4.6E18) always reconstruct; an unlucky prime resets M instead. The checks stay as the defence; reported.)
    "poly-remainder-not-reduced": [
        ("src/casim.c", "    for (int i = nf; i >= ng; i--) {\n        if (!rat_mul(&q[i - ng], &r[i], &inv)) return false;",
         "    for (int i = nf; i >= ng + 1; i--) {\n        if (!rat_mul(&q[i - ng], &r[i], &inv)) return false;"),
        ("src/casim.c", "    if (rdeg(r) >= ng) return false;", "    (void)0;"),
    ],
    "poly-gcd-sign-not-normalized": [
        ("src/casim.c", "        for (int i = 0; i <= best; i++) cand->a[i] /= cand->a[best] < 0 ? -ct : ct;",
         "        for (int i = 0; i <= best; i++) cand->a[i] /= ct;"),
    ],
    "poly-cancel-rows-dropped": [
        ("src/casim.c", "                removed_rows(&c, &w, gd, var, (solve_ws *)(void *)nd(&c, wr), (int32_t *)(void *)nd(&c, dqr), P);",
         "                (void)removed_rows, (void)dqr;"),
    ],
    # CAS 005 rule 2 as amended: the equation as typed
    "solve-denominator-allowed": [
        ("src/casim.c", "        if (z->kind == K_BDIV && raw_has_var(c, z->b, var)) {", "        if (z->kind == K_BDIV && raw_has_var(c, z->b, var) && 0) {"),
    ],
    "solve-function-of-var-numeric": [
        ("src/casim.c", "        if (raw_has_var(c, r, var)) c->st = CASIM_NOT_POLYNOMIAL;",
         "        if ((raw_has_var(c, r, var), 0)) c->st = CASIM_NOT_POLYNOMIAL;"),
    ],
    "no-room-as-too-long": [
        ("src/casim.c", "    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_NO_ROOM, c->starved = true;   /* the arena */",
         "    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_TOO_LONG;"),
        ("src/casim.c", "        if (c->st == CASIM_OK) c->st = CASIM_NO_ROOM;          /* the stack's bound (CAS 004: not TOO_LONG) */",
         "        if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;"),
    ],
    # (zero-half-level, CAS 004's review: 0^(-1/2) printed with its ^. Since CAS 008 rule 5 (uniform), power() refuses a
    # numeric 0 under a negative power before a node exists, and below() refuses only that base, so the clause is
    # unreachable: the mutant is equivalent. The clause stays as a defence; stamp_test's Y^(2^-0.5) chains keep the
    # printer's negative-half-power case.)
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
    # cas-pi-form: π's scope. Bare π not a fraction (0.25×π again); π under a ÷ or a root a fraction (1÷2÷π)
    "pi-frac-bare-pi": [
        ("src/casim.c", "        if (x->kind == K_PI) return true;", "        if (x->kind == K_PI) return false;"),
    ],
    "pi-frac-under-divide": [
        ("src/casim.c", "&& is_num(c, x->b) && val(c, x->b)->q == 1 &&\n            val(c, x->b)->p > 0)", ")"),
    ],
    # cas-pi-form: the logs. Combining off; the gcd not put back outside (2×LN(2)+2×LN(3) as LN(6)); the gcd taken
    # from the first coefficient (3×LN(2)-LN(4) left apart); groups across denominators (LN(8)+LN(4)÷3 as
    # LN(2048)÷3); LN(1÷m) losing its sign; LOG(10^-j) as j
    "logs-not-combined": [
        ("src/casim.c", "    if (combine_logs(c, t, &k)) {", "    if (0 && combine_logs(c, t, &k)) {"),
    ],
    "log-gcd-not-outside": [
        ("src/casim.c", "            } else if (!rat_is(&g, 1, 1))\n                r = mul2(c, numm(c, &g, gm), r);", "            }"),
    ],
    "log-gcd-first": [
        ("src/casim.c", "                    if (!rat_gcd(&g, &g, &kj)) return false;",
         "                    if (g.p == 0 && !rat_gcd(&g, &g, &kj)) return false;"),
    ],
    "log-groups-mixed": [
        ("src/casim.c", "    return log_term(c, t, k, a) == f && k->q == den;", "    return log_term(c, t, k, a) == f && den;"),
    ],
    "log-sign-lost": [
        ("src/casim.c", "    return neg_ ? mul_node(c, small(c, C_M1), &r, 1) : r;", "    return neg_ ? r : r;"),
    ],
    # cas-pi-form: a log of a letter taken into a group (its argument read as a number): letters must never combine
    "log-letters-combined": [
        ("src/casim.c", "!is_num(c, x->a) || val(c, x->a)->p <= 0) return -1;",
         "(is_num(c, x->a) && val(c, x->a)->p <= 0)) return -1;"),
    ],
    # show steps (work/steps-derive.md): deriv given a frame of its own; the walk counting a node's level twice (deep
    # derivatives page NO_ROOM); a product template losing its d/dX, or its denominators' ÷; the general power's
    # divisor printed as its base; two rules swapped; one-line order not ending on the answer; a step's result looked up
    # wrongly; steps recorded with steps off (plain derive's arena grows)
    "steps-deriv-noinline": [
        ("src/casim.c", "static inline __attribute__((always_inline)) ref deriv(ctx *c, ref r)\n{",
         "static __attribute__((noinline)) ref deriv(ctx *c, ref r)\n{"),
        ("src/casim.c", "static inline __attribute__((always_inline)) ref deriv(ctx *c, ref r);",
         "static __attribute__((noinline)) ref deriv(ctx *c, ref r);"),
    ],
    "steps-depth-twice": [
        ("src/casim.c", "    if (!dep(c, r)) return CASIM_RULE_CONSTANT;",
         "    if (!dep(c, r) || !contains(c, r, c->var)) return CASIM_RULE_CONSTANT;"),
    ],
    "steps-product-no-dvar": [
        ("src/casim.c", "            if (i == j) put_dvar(c, w, list(c, x->a)[i]);", "            if (i == j) put_factor(c, w, list(c, x->a)[i], 2);"),
    ],
    "steps-product-denominators": [
        ("src/casim.c", "            if (i != j && below(c, list(c, x->a)[i])) continue;", "            if (0) continue;"),
    ],
    "steps-general-power-divisor": [
        ("src/casim.c", "        put_whole(c, w, x->a, 5);", "        put_factor(c, w, x->a, 3);"),
    ],
    "steps-rules-swapped": [
        ("src/casim.c", "        return n == 1 ? CASIM_RULE_CONSTANT_MULTIPLE : CASIM_RULE_PRODUCT;",
         "        return n == 1 ? CASIM_RULE_PRODUCT : CASIM_RULE_CONSTANT_MULTIPLE;"),
    ],
    "steps-one-line-first": [
        ("src/casim.c", "    if (tmpl && !one) emit(W, r, rule, depth, CASIM_STEP_TEMPLATE);",
         "    if (tmpl && !one) emit(W, r, rule, depth, CASIM_STEP_TEMPLATE);\n    if (one) emit(W, r, rule, depth, CASIM_STEP_RESULT);"),
        ("src/casim.c", "    emit(W, r, rule, depth, CASIM_STEP_RESULT);         /* both ways the walk ends on the answer (abacus #6252) */",
         "    if (!one) emit(W, r, rule, depth, CASIM_STEP_RESULT);"),
    ],
    "steps-after-lookup": [
        ("src/casim.c", "        if (step_at(c, s)->before == r) return step_at(c, s)->after;",
         "        if (step_at(c, s)->before != r) return step_at(c, s)->after;"),
    ],
    "steps-recorded-off": [
        ("src/casim.c", "    if (c->steps) note(c, r, v);", "    note(c, r, v);"),
    ],
    # show steps 2 (integrate, work/steps-integrate.md): a factor's result run again with the sign flipped (a recompute
    # that differs from integrate's own answer); the substitution's ÷a taking b; a SUM template's signs flipped; a
    # divisor's 1÷ lost in CONSTANT MULTIPLE's template; no CHECK lines; the typed printer losing parentheses; the
    # cached constants kept past the arena's reset; the root's result line missing
    "isteps-recompute-sign": [
        ("src/casim.c", "    return integ_factor(c, t->before, t->flags & 2 ? -1 : 1, c->var);",
         "    return integ_factor(c, t->before, t->flags & 2 ? 1 : -1, c->var);"),
    ],
    "isteps-subst-a": [
        ("src/casim.c", "        put_whole(c, w, num(c, &lin[1]), 5);", "        put_whole(c, w, num(c, &lin[0]), 5);"),
    ],
    "isteps-sum-signs": [
        ("src/casim.c", "        puts_(w, (nd(c, z)->kind == K_BSUB ? -sign : sign) < 0 ? \"-\" : \"+\");",
         "        puts_(w, (nd(c, z)->kind == K_BSUB ? sign : -sign) < 0 ? \"-\" : \"+\");"),
    ],
    "isteps-cm-divisor": [
        ("src/casim.c", "    put_raw_integral(c, w, g, s < 0);", "    put_raw_integral(c, w, g, s < 0 && 0);"),
    ],
    "isteps-no-check": [
        ("src/casim.c", "    if (rec && checked) irecord(c, depth, CASIM_RULE_CHECK, CASIM_STEP_RESULT, t, 1, VIA_TERM);",
         "    if (0 && rec && checked) irecord(c, depth, CASIM_RULE_CHECK, CASIM_STEP_RESULT, t, 1, VIA_TERM);"),
    ],
    "isteps-raw-parens": [
        ("src/casim.c", "    bool paren = raw_level(c, r) < min || (nd(c, r)->kind == K_NEG && !lead);",
         "    bool paren = raw_level(c, r) < min - 1 || (nd(c, r)->kind == K_NEG && !lead);"),
    ],
    "isteps-stale-constants": [
        ("src/casim.c", "        if (c->small[j] >= parsed / 8u) c->small[j] = 0;", "        if (0) c->small[j] = 0;"),
    ],
    "isteps-no-root-result": [
        ("src/casim.c", "    if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, r0, 1, VIA_SUM);",
         "    if (rec && own && depth) irecord(c, depth, rule, CASIM_STEP_RESULT, r0, 1, VIA_SUM);"),
    ],
    # abacus #6252: a monomial's result through integ_factor (which refuses rule 6a's SQRT(X)^3, INV(X^2)) instead of
    # the polynomial path integrate took: the walk refuses where integrate answers; the corpus check must see it
    "isteps-mono-via-factor": [
        ("src/casim.c", "                if (rec) irecord(c, depth + 1, r1, CASIM_STEP_RESULT, g, s, VIA_MONO);",
         "                if (rec) irecord(c, depth + 1, r1, CASIM_STEP_RESULT, g, s, VIA_FACTOR);"),
    ],
    # show steps, 3: solve (work/steps-solve.md; abacus #6288 asked for the sort, a skipped skip, a missed factor step
    # and a dropped multiplicity)
    "ssteps-linear-order": [
        ("src/casim.c", "    if (x->deg == 1) return x->b * y->a < y->b * x->a;", "    if (x->deg == 1) return x->b * y->a > y->b * x->a;"),
    ],
    "ssteps-quadratic-order": [
        ("src/casim.c", "    return x->a != y->a ? x->a < y->a : x->b != y->b ? x->b < y->b : x->c < y->c;",
         "    return x->a != y->a ? x->a < y->a : x->b != y->b ? x->b > y->b : x->c < y->c;"),
    ],
    "ssteps-whole-not-skipped": [
        ("src/casim.c", "            if (whole) continue;", "            if (whole && 0) continue;"),
    ],
    "ssteps-missed-factor": [
        ("src/casim.c", "            if (ss_hit(S, w, 0, CASIM_STEP_RESULT, rule)) ss_product(w, var, zero, W->fs, i + 1, &W->cur, i == nf - 1);",
         "            if (i != 1 && ss_hit(S, w, 0, CASIM_STEP_RESULT, rule)) ss_product(w, var, zero, W->fs, i + 1, &W->cur, i == nf - 1);"),
    ],
    "ssteps-product-mult": [
        ("src/casim.c", "        ss_ipoly(w, &p, var, true);\n        if (fs[i].mult > 1) puts_(w, \"^\"), put_int(w, fs[i].mult);",
         "        ss_ipoly(w, &p, var, true);\n        if (fs[i].mult > 2) puts_(w, \"^\"), put_int(w, fs[i].mult - 1);"),
    ],
    "ssteps-root-mult": [
        ("src/casim.c", "                rational_root(W->r, &nr, x->b, x->a, x->mult);",
         "                rational_root(W->r, &nr, x->b, x->a, x->mult > 1 ? x->mult - 1 : 1);"),
    ],
    "ssteps-discriminant": [
        ("src/casim.c", "            ss_val(w, x->b), puts_(w, \"^2-4\")", "            ss_val(w, x->b), puts_(w, \"^2+4\")"),
    ],
    "ssteps-formula-repeats": [
        ("src/casim.c", "    if (!(k == 1 && r > 1 && x->mult == 1) && ss_hit(S, w, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_QUADRATIC_FORMULA)) {",
         "    if (ss_hit(S, w, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_QUADRATIC_FORMULA)) {"),
    ],
    "ssteps-move-as-typed": [
        ("src/casim.c", "        if (!t.full && strcmp(W->t, typed) != 0 &&", "        if (!t.full &&"),
    ],
    "ssteps-multiplier": [
        ("src/casim.c", "            if (m.q == 1) puts_(w, TIMES), ss_val(w, m.p);", "            if (m.q == 1) puts_(w, TIMES), ss_val(w, m.p + 1);"),
    ],
    "ssteps-division-of-what-is-left": [
        ("src/casim.c", "                ss_ipoly(w, &W->before, var, true), puts_(w, DIVIDE), ss_ipoly(w, &p, var, true);",
         "                ss_ipoly(w, &W->g, var, true), puts_(w, DIVIDE), ss_ipoly(w, &p, var, true);"),
    ],
    "ssteps-zero-mult": [
        ("src/casim.c", "            puts_(w, \"=0 (MULT \"), put_int(w, zero), puts_(w, \")\");", "            puts_(w, \"=0\");"),
    ],
    # abacus #6311: X alone has no root line; a power left prints as one
    "ssteps-root0-alone": [
        ("src/casim.c", "        if (zero > 1 && ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LINEAR)) {",
         "        if (zero && ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LINEAR)) {"),
    ],
    "ssteps-power-k": [
        ("src/casim.c", "    if (rest->n >= 2 && ss_power(rest, &r, &k)) ss_ipoly(w, &r, var, true), puts_(w, \"^\"), put_int(w, k);",
         "    if (rest->n >= 2 && ss_power(rest, &r, &k)) ss_ipoly(w, &r, var, true), puts_(w, \"^\"), put_int(w, k + 1);"),
    ],
    "ssteps-left-expanded": [
        ("src/casim.c", "            ss_left(w, &W->cur, var, false), puts_(w, \"=0: DEGREE \"), put_int(w, W->cur.n), puts_(w, \" LEFT\");",
         "            ss_ipoly(w, &W->cur, var, false), puts_(w, \"=0: DEGREE \"), put_int(w, W->cur.n), puts_(w, \" LEFT\");"),
    ],
    "ssteps-line-in-slot": [
        ("src/casim.c", "static bool ss_hit(sstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (line == CASIM_STEP_TEMPLATE && S->one) return false;\n    if (S->at++ != S->k) return false;\n    S->info->depth = depth, S->info->line = line, S->info->rule = rule;\n    if (rule != CASIM_RULE_ANSWER) {\n        w->cap = S->cap, w->n = 0, w->full = false;",
         "static bool ss_hit(sstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (line == CASIM_STEP_TEMPLATE && S->one) return false;\n    if (S->at++ != S->k) return false;\n    S->info->depth = depth, S->info->line = line, S->info->rule = rule;\n    if (rule != CASIM_RULE_ANSWER) {\n        w->cap = S->cap < SLOT ? S->cap : SLOT, w->n = 0, w->full = false;"),
    ],
    "ssteps-answer-page": [
        ("src/casim.c", "            puts_(&w, ans);", "            puts_(&w, ans + 1);"),
    ],
    "ssteps-one-line-template": [
        ("src/casim.c", "static bool ss_hit(sstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (line == CASIM_STEP_TEMPLATE && S->one) return false;\n    if (S->at++ != S->k) return false;",
         "static bool ss_hit(sstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (line == CASIM_STEP_TEMPLATE && S->one && rule != CASIM_RULE_INTEGER_COEFFICIENTS) return false;\n    if (S->at++ != S->k) return false;"),
    ],
    "log10-exact-sign": [
        ("src/casim.c", "        rat_int(&m, (int)(q->q == 1 ? j : -j));", "        rat_int(&m, (int)j);"),
    ],
    # 012 (abacus's list, "a numerator not reduced below deg f"): the digits in base f always have degree below f's, so
    # the nearest fault the code can make is the product A = r s left unreduced mod P; rule 7's check must refuse it
    "pf-num-not-reduced": [
        ("src/casim.c", "        if (lead.p != 0) {\n            if (!pf_rmul(&lead, &lead, &inv)) return false;",
         "        if (0) {\n            if (!pf_rmul(&lead, &lead, &inv)) return false;"),
    ],
    # 012: the base-f digits stored in reverse, the numerator over f^k as the one over f
    "pf-digits-reversed": [
        ("src/casim.c", "W->pool[used + (j - 1) * df + x] = W->t[1][x];", "W->pool[used + (F->k - j) * df + x] = W->t[1][x];"),
    ],
    # 012: a term's sign lost
    "pf-sign-flipped": [         # anchored at pf_term's own header: the steps' twin, irs_pterm, has the same body
        ("src/casim.c", 'static bool pf_term(out_t *w, const rat *num, const rat *f, int j, int var, bool first, rat *ip)\n{\n    int64_t l = 1, g = 0;\n    int dn = rdeg(num), terms = 0;\n    rat k;\n    for (int i = 0; i <= dn; i++)\n        if (num[i].p != 0 && !mul64(&l, l, num[i].q / gcd64(l, num[i].q))) return false;\n    pzero(ip);\n    for (int i = 0; i <= dn; i++) {\n        if (num[i].p != 0 && !mul64(&ip[i].p, num[i].p, l / num[i].q)) return false;\n        g = gcd64(g, ip[i].p);\n    }\n    if (ip[dn].p < 0) g = -g;\n    for (int i = 0; i <= dn; i++) ip[i].p /= g, terms += ip[i].p != 0;\n    if (!pf_rmake(&k, g, l)) return false;                       /* num = k N */\n    if (k.p < 0) pf_puts(w, "-"), k.p = -k.p;\n    else if (!first) pf_puts(w, "+");',
         'static bool pf_term(out_t *w, const rat *num, const rat *f, int j, int var, bool first, rat *ip)\n{\n    int64_t l = 1, g = 0;\n    int dn = rdeg(num), terms = 0;\n    rat k;\n    for (int i = 0; i <= dn; i++)\n        if (num[i].p != 0 && !mul64(&l, l, num[i].q / gcd64(l, num[i].q))) return false;\n    pzero(ip);\n    for (int i = 0; i <= dn; i++) {\n        if (num[i].p != 0 && !mul64(&ip[i].p, num[i].p, l / num[i].q)) return false;\n        g = gcd64(g, ip[i].p);\n    }\n    if (ip[dn].p < 0) g = -g;\n    for (int i = 0; i <= dn; i++) ip[i].p /= g, terms += ip[i].p != 0;\n    if (!pf_rmake(&k, g, l)) return false;                       /* num = k N */\n    if (k.p < 0) k.p = -k.p;\n    if (!first) pf_puts(w, "+");'),
    ],
    # 012: the polynomial part of an improper fraction dropped
    "pf-poly-part-dropped": [
        ("src/casim.c", "    if (rdeg(W->poly) >= 0) pf_rpoly(&w, W->poly, var), first = false;",
         "    if (0) pf_rpoly(&w, W->poly, var), first = false;"),
    ],
    # 012: CANCEL's rows dropped
    "pf-rows-dropped": [
        ("src/casim.c", "            if (F->gone == 0) continue;\n            if (F->deg == 1) {",
         "            continue;\n            if (F->deg == 1) {"),
    ],
    # 012: rule 7's identity check skipped (only tests/pfcheck_check's spoiled numerators reach it)
    "pf-check-skipped": [
        ("src/casim.c", "            if (pf_rcmp(&W->t[4][x], &W->t[5][x]) != 0) return CASIM_NO_ROOM;     /* never printed unchecked */",
         "            if (0) return CASIM_NO_ROOM;     /* never printed unchecked */"),
    ],
    # 012: rule 5's sort skipped: the factors in factor's own order
    "pf-sort-skipped": [
        ("src/casim.c", "        for (int i = 1; i < W->nf; i++)\n            for (int j = i; j > 0 && pf_cmp(",
         "        for (int i = W->nf; i < W->nf; i++)\n            for (int j = i; j > 0 && pf_cmp("),
    ],
    # 012's sibling (abacus #6437): CANCEL's 0÷den back to NO_ROOM
    "cancel-zero-numerator": [
        ("src/casim.c", "            if (fi->n < 0) {\n                /* 0÷den", "            if (0) {\n                /* 0÷den"),
    ],
    # partfrac steps: CLEARED's cofactor with F to its full power
    "psteps-cofactor-power": [
        ("src/casim.c", "        int p = x == i ? W->fac[x].k - j : W->fac[x].k;", "        int p = x == i ? W->fac[x].k - 0 * j : W->fac[x].k;"),
    ],
    # partfrac steps: the cover-up values not moved across in an equation
    "psteps-known-not-moved": [
        ("src/casim.c", "                        if (P->known[u]) pf_fma(&left, &cm[u], &P->val[u], true);",
         "                        if (0) pf_fma(&left, &cm[u], &P->val[u], true);"),
    ],
    # partfrac steps: a cover-up result with the first unknown's value
    "psteps-cover-up-value": [
        ("src/casim.c", "CASIM_RULE_COVER_UP)) ps_letter(w, P->letter[u]), pf_puts(w, \"=\"), ps_rat(w, &P->val[u]);",
         "CASIM_RULE_COVER_UP)) ps_letter(w, P->letter[u]), pf_puts(w, \"=\"), ps_rat(w, &P->val[0]);"),
    ],
    # partfrac steps: the highest power's equation left out
    "psteps-top-equation": [
        ("src/casim.c", "            for (int m = rdeg(W->d) - 1; m >= 0; m--) {\n                if (ps_hit(",
         "            for (int m = rdeg(W->d) - 2; m >= 0; m--) {\n                if (ps_hit("),
    ],
    # partfrac steps: PARTIAL lost on the pages
    "psteps-partial-ok": [
        ("src/casim.c", "    return st;                                           /* PARTIAL on every page",
         "    return CASIM_OK;                                     /* PARTIAL on every page"),
    ],
    # partfrac steps: the answer page not partfrac's text
    "psteps-answer-page": [
        ("src/casim.c", "            pf_puts(&w, ans);", "            pf_puts(&w, ans + 1);"),
    ],
    # partfrac steps: one-line pages with the templates
    "psteps-one-line": [
        ("src/casim.c", "static __attribute__((noinline)) bool ps_hit(pstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (line == CASIM_STEP_TEMPLATE && S->one) return false;",
         "static __attribute__((noinline)) bool ps_hit(pstep_t *S, out_t *w, int depth, int line, int rule)\n{\n    if (0) return false;"),
    ],
    # 013 (abacus's list): a term's check skipped at both call sites (tests/ircheck_check's spoiled terms reach it; the
    # macro's default is not the place: ircheck_check defines its own)
    "irat-check-skipped": [
        ("src/casim.c", "if (c.st != CASIM_OK || !CASIM_IR_CLOSES(&c, F, f)) return c.st == CASIM_OK",
         "if (c.st != CASIM_OK || ((void)f, 0)) return c.st == CASIM_OK"),
        ("src/casim.c", "                    if (c.st != CASIM_OK || !CASIM_IR_CLOSES(&c, F, f))\n",
         "                    if (c.st != CASIM_OK || ((void)f, 0))\n"),
    ],
    # 013: ABS dropped on a linear factor's LN (the check closes without it: the text moves)
    "irat-abs-dropped": [
        ("src/casim.c", "        if (j == 1) return mul2(c, ir_rat(c, nm[0].p, q), fn(c, F_LN, fn(c, F_ABS, ft)));",
         "        if (j == 1) return mul2(c, ir_rat(c, nm[0].p, q), fn(c, F_LN, ft));"),
    ],
    # 013: ATAN's angle factor dropped
    "irat-angle-dropped": [
        ("src/casim.c", "mul2(c, fn(c, F_ATAN, mul2(c, u, power(c, root, small(c, C_M1)))), angle_factor(c, false)));",
         "mul2(c, fn(c, F_ATAN, mul2(c, u, power(c, root, small(c, C_M1)))), small(c, C_1)));"),
    ],
    # 013: ATAN's argument off by its 2a
    "irat-atan-2a": [
        ("src/casim.c", "        u = add2(c, mul2(c, ir_rat(c, 2 * a, 1), x), ir_rat(c, b, 1));    /* 2aX + b */",
         "        u = add2(c, mul2(c, ir_rat(c, a, 1), x), ir_rat(c, b, 1));    /* 2aX + b */"),
    ],
    # 013: a refused form let through (D > 0 and quadratic powers into the D < 0 formula; ircheck_check without the
    # check sees it)
    "irat-refused-let-through": [
        ("src/casim.c", "    if (F->deg == 2 && j == 1 && D < 0) {", "    if (F->deg == 2) {"),
    ],
    # 013: the hook moved inside do_integrate (the frame table catches it)
    "irat-hook-in-do-integrate": [
        ("src/casim.c", "    if (st == CASIM_NOT_INTEGRABLE) st = do_irational(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);\n",
         ""),
        ("src/casim.c", "    if (c.st == CASIM_OK) F = integ(&c, n, var);         /* each term checked as it goes (integ_checked) */\n    return finish(&c, F, out, cap < SLOT ? cap : SLOT);",
         "    if (c.st == CASIM_OK) F = integ(&c, n, var);         /* each term checked as it goes (integ_checked) */\n    if (c.st == CASIM_NOT_INTEGRABLE) return do_irational(eqn, var, angle, out, cap, arena, arena_cap, rtl);\n    return finish(&c, F, out, cap < SLOT ? cap : SLOT);"),
    ],    # 013 steps (§S4): a quadratic part's proof skipped, the ATAN part's (ircheck_check's spoiled parts reach it)
    "irs-check-skipped": [
        ("src/casim.c", '        if (!CASIM_IRS_CLOSES(c, Fp, fp)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;\n        if (irs_hit(S, dn, CASIM_STEP_TEMPLATE, CASIM_RULE_LOG_DERIVATIVE))',
         '        if (!((void)fp, 1)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;\n        if (irs_hit(S, dn, CASIM_STEP_TEMPLATE, CASIM_RULE_LOG_DERIVATIVE))'),
    ],
    # the LN part's proof skipped
    "irs-check-skipped-ln": [
        ("src/casim.c", '        if (!CASIM_IRS_CLOSES(c, Fp, fp)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;\n        if (square && irs_hit',
         '        if (!((void)fp, 1)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;\n        if (square && irs_hit'),
    ],
    # SPLIT's k2 off (k2 = 2C): the template's value moves
    "irs-k2-off": [("src/casim.c", '!pf_radd(&k2, &nm[0], &t)', '!pf_radd(&k2, &nm[0], &nm[0])')],
    # the angle factor dropped from the ARCTANGENT part in DEG and GRAD (built in RAD; its proof still closes)
    "irs-deg-dropped": [("src/casim.c", '        Fp = ir_term(c, F, 1, z, T + 2 * PF_N, var);\n', '        c->angle = CASIM_RAD, Fp = ir_term(c, F, 1, z, T + 2 * PF_N, var);\n')],
    # ABS added to LOG DERIVATIVE's LN (it still closes: the judge's rule-4 check sees it)
    "irs-abs-added": [("src/casim.c", '        Fp = fn(c, F_LN, poly_tree(c, f, var));\n', '        Fp = fn(c, F_LN, fn(c, F_ABS, poly_tree(c, f, var)));\n')],
    # S2.3's skip widened to every monic quadratic (b != 0 too)
    "irs-square-skip-widened": [("src/casim.c", 'square = !(b == 0 && a == 1);', "square = !(a == 1);")],
    # the chained-power check taken away while a chain prints: the values still see it
    "irs-chain-unchecked": [
        ("src/casim.c", '            pf_puts(S->w, "^2+"), ps_rat(S->w, &kk);', '            pf_puts(S->w, "^1^2+"), ps_rat(S->w, &kk);'),
        ("scripts/istep-values.py", 'CHAIN = re.compile(r"\\^(\\d+(\\.\\d+)?|\\([^()]*\\))\\^")', 'CHAIN = re.compile(r"(?!x)x")'),
    ],
    # 014 (work/014-taylor.md's mutants): k! as (k-1)!
    "taylor-factorial-short": [("src/casim.c", '        if (k > 0 && !rat_mul(&kf, &kf, &(rat){k, 1})) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);',
                                '        if (k > 0 && !rat_mul(&kf, &kf, &(rat){k > 1 ? k - 1 : 1, 1})) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);')],
    # the sign of a in (X-a) flipped: the values are a's, the powers (X+a)'s
    "taylor-point-sign": [("src/casim.c", '    xa = at0 ? leaf(&c, K_VAR, var) : canon(&c, add2(&c, leaf(&c, K_VAR, var), neg(&c, a)));', '    xa = at0 ? leaf(&c, K_VAR, var) : canon(&c, add2(&c, leaf(&c, K_VAR, var), a));')],
    # the angle factor dropped in DEG and GRAD: the derivatives taken in RAD
    "taylor-angle-dropped": [("src/casim.c", '    start(&c, eqn, angle, arena, arena_cap);\n    c.rtl = rtl;\n    c.var = var;                        /* the names derive refuses are refused here too', '    start(&c, eqn, CASIM_RAD, arena, arena_cap);\n    c.rtl = rtl;\n    c.var = var;                        /* the names derive refuses are refused here too')],
    # a zero term printed
    "taylor-zero-printed": [("src/casim.c", '    if (is_k(c, ck, 0, 1)) return;\n    if (k == 0) t = ck;', '    if (k == 0) t = ck;')],
    # one order short (the loop stops at n-1)
    "taylor-one-short": [("src/casim.c", '    for (int k = 0; k <= n && c.st == CASIM_OK; k++) {\n        ref v;\n        rat inv;', '    for (int k = 0; k < n && c.st == CASIM_OK; k++) {\n        ref v;\n        rat inv;')],
    # the point rounded to a 16-digit decimal before the values are taken
    "taylor-point-rounded": [("src/casim.c", '        a = c.st == CASIM_OK ? read_bound(&c, point, chars + 1, &av) : SPARE;    /* rule 3 */', "        a = c.st == CASIM_OK ? read_bound(&c, point, chars + 1, &av) : SPARE;    /* rule 3 */\n        if (c.st == CASIM_OK) {\n            char b[40];\n            rat q;\n            abn r;\n            abn_to_text(&av, b, sizeof b);\n            b[18] = '\\0';\n            if (abn_from_text(&r, b) == ABN_OK && rat_from_abn(&q, &r)) a = num(&c, &q);\n        }")],
    # rule 5 at TAN's poles: COS not taken of TAN's argument, so TAN(π÷2) is printed as a value
    "taylor-tan-pole-skipped": [("src/casim.c", "        if (tan) u = canon(c, fn(c, F_COS, u));                     /* a pole: COS is 0 there */\n", "")],
    # rule 7: no value check at a (LN(0), ABS's corner and a 0 divided printed as values)
    "taylor-hole-skipped": [("src/casim.c", "    if (c->st == CASIM_OK && taylor_hole(c, f, var, a)) c->st = CASIM_UNDEFINED;", "    if (c->st == CASIM_OK && taylor_hole(c, f, var, a) && c->var < 0) c->st = CASIM_UNDEFINED;")],
    # rule 7, the typed f: its parts not valued before canon cancels them (X÷X at 0 printed as 1)
    "taylor-typed-check-skipped": [("src/casim.c", "        taylor_check(&c, raw, var, a);\n", "")],
    # the round trip: an opaque SGN not read back as derive's, so ABS's derivatives keep an opaque SGN(X)
    "taylor-sgn-opaque": [("src/casim.c", "    if (c->own_text && !b && strcmp(NAMES[nd(c, r)->id].name, \"SGN\") == 0)\n        return fn(c, F_SGN, a);", "    if (c->own_text && !b && strcmp(NAMES[nd(c, r)->id].name, \"SGX\") == 0)\n        return fn(c, F_SGN, a);")],
    # the ^2 taken from the term's first bracket, not (X-a)'s: SIN(1)^3's coefficient loses its own power
    "taylor-strip-first-bracket": [("src/casim.c", "            if (p[0] == '(' && p[1] == 'A' + c->var && (p[2] == '-' || p[2] == '+')) {", "            if (p[0] == '(') {")],
    # the names derive refuses taken as opaque (X! printed as its own Taylor polynomial)
    "taylor-names-opaque": [("src/casim.c", "    c.var = var;                        /* the names derive refuses are refused here too (not opaque): it derives */", "    c.var = var, c.opaque = true;")],
    # CAS 015, the unit's six. L'Hôpital applied to a quotient that isn't indeterminate (finite ÷ finite nonzero)
    "limit-lhopital-not-indeterminate": [("src/casim.c", "                    return lm(L_FIN, canon(c, mul2(c, ln.v, power(c, ld.v, small(c, C_M1)))));\n", "")],
    # the one-sided sign rule flipped: an odd derivative's sign taken as it is from below
    "limit-side-sign-flipped": [("src/casim.c", "        return L->side > 0 || m % 2 == 0 ? s : -s;", "        return L->side > 0 || m % 2 == 0 ? -s : s;")],
    # ATAN's ends without the angle factor: π÷2 in DEG
    "limit-atan-angle-dropped": [("src/casim.c", "lu.k == L_PINF ? C_HALF : C_MHALF), leaf(c, K_PI, 0)),\n                                         angle_factor(c, true))));", "lu.k == L_PINF ? C_HALF : C_MHALF), leaf(c, K_PI, 0)),\n                                         small(c, C_1))));")],
    # b^e with var in e not rewritten as EXP(e×LN(b)): 1^∞ undecided
    "limit-exp-ln-dropped": [("src/casim.c", "        r = lm_walk(c, canon(c, fn(c, F_EXP, mul2(c, nd(c, f)->b, fn(c, F_LN, nd(c, f)->a)))), L);", "        r = lm(L_UND, 0);")],
    # the step cap one short
    "limit-step-cap-short": [("src/casim.c", "        if (++L->steps > LIM_STEPS || c->st != CASIM_OK) return lm(L_UND, 0);", "        if (++L->steps >= LIM_STEPS || c->st != CASIM_OK) return lm(L_UND, 0);")],
    # UNDECIDED reported as UNDEFINED ("no limit")
    "limit-undecided-as-undefined": [("src/casim.c", "    return r.k == L_UND ? CASIM_UNDECIDED : CASIM_UNDEFINED;", "    return CASIM_UNDEFINED;")],
    # a review's four: the conjugate's result without the sum's finite terms
    "limit-conjugate-drops-finite": [("src/casim.c", "            if (r.k == L_FIN) r.v = canon(c, add2(c, r.v, fin));      /* the sum's finite terms beside p + m */\n", "            (void)fin;\n")],
    # SGN valued at its argument's limit (SGN(0) is 0) instead of its argument's sign near the point
    "limit-sgn-by-value": [("src/casim.c", "        if (sg == 0) sg = lm_sign_near(c, u, L);\n", "")],
    # an exponent whose sign isn't known taken as positive (X^K at ∞ is ∞)
    "limit-unknown-exponent-positive": [("src/casim.c", "r = se == 1 ? lm(L_PINF, 0) : se == -1 ? lm(L_FIN, small(c, C_0)) : lm(L_UND, 0);", "r = se != -1 ? lm(L_PINF, 0) : lm(L_FIN, small(c, C_0));")],
    # two oscillations taken as proven to have no limit (SIN(X)+SIN(X+π) is 0)
    "limit-oscillations-summed": [("src/casim.c", "    if (none || osc) return osc + none == 1 && !pinf && !ninf ? lm(L_NONE, 0) : lm(L_UND, 0);", "    if (none || osc) return !pinf && !ninf ? lm(L_NONE, 0) : lm(L_UND, 0);")],
    # CAS 016: TAN at its poles kept (TAN(π÷2) as if it had a value)
    "pole-tan-dropped": [("src/casim.c", "        if (f == F_TAN && table_val(F_COS, n, &v) && v.p == 0)\n            return c->st = CASIM_UNDEFINED, SPARE;                  /* CAS 016 rule 1: COS is 0 there, a pole */\n", "")],
    # CAS 016: LN(0) kept
    "pole-ln-dropped": [("src/casim.c", "        if (is_k(c, a, 0, 1)) return c->st = CASIM_UNDEFINED, SPARE;     /* CAS 016 rule 1: a pole, as 1÷0 is */\n", "")],
    # CAS 016: LOG(0) kept
    "pole-log-dropped": [("src/casim.c", "        if (is_k(c, a, 0, 1)) return c->st = CASIM_UNDEFINED, SPARE;     /* CAS 016 rule 1 */\n", "")],
    # CAS 016: ATANH(±1) kept
    "pole-atanh-dropped": [("src/casim.c", "        if (is_k(c, a, 1, 1) || is_k(c, a, -1, 1)) return c->st = CASIM_UNDEFINED, SPARE;   /* CAS 016 rule 1 */\n", "")],
    # CAS 016: pw's own pole check dropped (cancel, partfrac and pdiv see TAN(π÷2) as NOT_NUMERIC)
    "pole-solve-not-numeric": [("src/casim.c", "            if (c->st != CASIM_UNDEFINED) c->st = CASIM_NOT_NUMERIC;\n", "            c->st = CASIM_NOT_NUMERIC;\n")],
    # CAS 016: SGN(0) cleared beside any factor, not only one that is 0 (2×SGN(X) at 0 answered)
    "sgn-product-any-factor": [("src/casim.c", "&& taylor_zero(c, y, var, a)) return nd(c, g)->a;\n", "&& (taylor_zero(c, y, var, a) || 1)) return nd(c, g)->a;\n")],
    # CAS 016: every SGN(0) the rule can't clear called UNDEFINED (ABS(X)^2's order 2, which exists)
    "corner-always-undefined": [("src/casim.c", "    return lone && g ? CASIM_UNDEFINED : CASIM_UNDECIDED;\n", "    return CASIM_UNDEFINED;\n")],
    # CAS 016: a sign change read from an even first derivative
    "corner-turn-parity": [("src/casim.c", "        if (!is_k(c, v, 0, 1)) return const_sign(c, v) ? m % 2 : -1;\n", "        if (!is_k(c, v, 0, 1)) return const_sign(c, v) ? (m + 1) % 2 : -1;\n")],
    # CAS 016: an ∞-∞ at a point that no term dominates left undecided (1÷X-1÷SIN(X))
    "combine-skipped": [("src/casim.c", "    if (!under || c->st != CASIM_OK) return lm(L_UND, 0);\n", "    if (under || !under) return lm(L_UND, 0);\n")],
    # CAS 016: the one-denominator rewrite applied to a sum whose terms all have limits
    "combine-finite-sum": [("src/casim.c", "    if (pinf) return lm(L_PINF, 0);\n    if (ninf) return lm(L_NINF, 0);\n    return lm(L_FIN, canon(c, s));\n", "    if (pinf) return lm(L_PINF, 0);\n    if (ninf) return lm(L_NINF, 0);\n    return L->a ? lm_combine(c, f, small(c, C_0), small(c, C_0), L, c->used, L->steps) : lm(L_FIN, canon(c, s));\n")],
    # CAS 016: the - side walked on top of the + side's work
    "limit-sides-share-arena": [("src/casim.c", "            else if (up.k == L_FIN) up.v = compact(&c, mark, up.v);     /* CAS 016: the - side gets the arena back */\n            else if (c.st == CASIM_OK) taylor_drop(&c, mark);\n", "            (void)mark;\n")],
    # CAS 016: the 1,024 B buffers from one byte past 16 KB
    "buffer-threshold-off-by-one": [("src/casim.c", "    tt = arena_cap >= TAYLOR_BIG_ARENA ? TAYLOR_TEXT_BIG : TAYLOR_TEXT;\n", "    tt = arena_cap > TAYLOR_BIG_ARENA ? TAYLOR_TEXT_BIG : TAYLOR_TEXT;\n")],
    # CAS 016: derive's walk of 0^u through LN(0), a pole (it ends UNDEFINED where derive answers 0)
    "steps-exp-zero-base": [("src/casim.c", "        t = is_k(c, x->a, 0, 1) ? small(c, C_0) : mul2(c, r, fn(c, F_LN, x->a));\n", "        t = mul2(c, r, fn(c, F_LN, x->a));\n")],
    # CAS 016: a lone corner claimed while another hole could cancel it (ABS(X)-SQRT(X^2) is 0)
    "corner-rest-ignored": [("src/casim.c", "    if (lone && g) lone = !taylor_rest_hole(c, f, t, g, var, a) && taylor_turns(c, nd(c, g)->a, var, a) == 1;\n", "    if (lone && g) lone = (taylor_rest_hole(c, f, t, g, var, a) || 1) && taylor_turns(c, nd(c, g)->a, var, a) == 1;\n")],
    # CAS 016: a 0÷0 in a derivative's formula called UNDEFINED (X×SQRT(X^2)'s order 1 exists)
    "taylor-00-undefined": [("src/casim.c", "        lone = k > 0 && taylor_00(c, f, var, a);\n", "        lone = k < 0 && taylor_00(c, f, var, a);\n")],
    # CAS 016: SINH(0), TANH(0), ASINH(0) kept, so LN(SINH(0)) hides its pole
    "hyperbolic-fold-dropped": [("src/casim.c", "        if (is_k(c, a, 0, 1)) return small(c, C_0);                 /* CAS 016 (abacus #7951): SINH(0), TANH(0), ASINH(0) */\n", "")],
    # CAS 016: solve's and factor's pole found only in the term pw reaches first (SIN(X)+TAN(π÷2)=0)
    "solve-pole-first-term": [("src/casim.c", "    s = pw_pole_(c, r, var) ? CASIM_UNDEFINED : s;\n", "    s = pw_pole_(c, r, var) && 0 ? CASIM_UNDEFINED : s;\n")],
    # CAS 016: CI's make test fetching the firmware's main, not CORE_PIN
    "ci-pin-from-main": [("scripts/ci-test.sh", "ref=$(cat CORE_PIN)\n", "ref=main\n")],
    # CAS 017: integrals by substitution and by parts
    "sub-check-skipped": [("src/casim.c", "if (c->st == CASIM_OK && ib_same(c, mul2(c, q, du), t)) {\n                        if (c->st == CASIM_OK) return c->var = v0, F;",
                           "if (c->st == CASIM_OK && (ib_same(c, mul2(c, q, du), t) || 1)) {\n                        if (c->st == CASIM_OK) return c->var = v0, F;")],
    "sgnabs-not-rewritten": [("src/casim.c", "nd(c, h->a)->id != F_ABS || cmp(c, nd(c, h->a)->a, g->a) != 0)\n                        continue;\n                    list(c, l)[i] = power(c, g->a, small(c, C_M1));\n                    list(c, l)[j] = small(c, C_1);",
                              "nd(c, h->a)->id != F_ABS || cmp(c, nd(c, h->a)->a, g->a) != 0)\n                        continue;\n                    (void)l;")],
    "parts-w-without-du": [("src/casim.c", "    w = ib_canon(c, mul2(c, v, du));", "    w = ib_canon(c, mul2(c, v, u)), (void)du;")],
    "cycle-one-plus-k": [("src/casim.c", "        omk = ib_canon(c, add2(c, small(c, C_1), ib_neg(c, k)));",
                          "        omk = ib_canon(c, add2(c, small(c, C_1), k));")],
    "loop-sign-dropped": [("src/casim.c", "            acc = add2(c, acc, j % 2 ? ib_neg(c, mul2(c, uj, V)) : mul2(c, uj, V));",
                           "            acc = add2(c, acc, (j % 2 || 1) ? mul2(c, uj, V) : ib_neg(c, mul2(c, uj, V)));")],
    "divisor-rule-off": [("src/casim.c",
                          "    if (c.st == CASIM_OK && !ib_typed_ok(&c, n, t, var) && c.st == CASIM_OK) c.st = CASIM_NOT_INTEGRABLE;\n    if (c.st == CASIM_OK) {\n        c.own_text = true;                      /* from here",
                          "    if (c.st == CASIM_OK && !ib_typed_ok(&c, n, t, var) && 0) c.st = CASIM_NOT_INTEGRABLE;\n    if (c.st == CASIM_OK) {\n        c.own_text = true;                      /* from here")],
    # after abacus's h017 (#8121): the divisor rule reading d (3×X^2+3) where t holds 3×(X^2+1); the constant left in the
    # rational path's text (π refuses it); the constant put back on the whole answer, not each term (the form moves)
    # CAS 017's steps build (work/017 §Steps S4)
    "steps-check-wrong-F": [("src/casim.c", "        pf_puts(w, d), pf_puts(w, P->s[1]), pf_puts(w, \") = \"), pf_puts(w, rq->eqn);",
                             "        pf_puts(w, d), pf_puts(w, P->s[0]), pf_puts(w, \") = \"), pf_puts(w, rq->eqn);")],
    "steps-parts-w-without-du": [("src/casim.c",
                                  "    B->how = IBH_PARTS, B->r[0] = u, B->r[1] = v, B->r[2] = ib_canon(c, mul2(c, v, ib_deriv(c, u)));",
                                  "    B->how = IBH_PARTS, B->r[0] = u, B->r[1] = v, B->r[2] = v;")],
    "steps-cycle-1mk-sign": [("src/casim.c", "            x[nx++] = ib_canon(&c, add2(&c, small(&c, C_1), ib_neg(&c, k)));",
                              "            x[nx++] = ib_canon(&c, add2(&c, small(&c, C_1), k));")],
    "steps-level-cap-off": [("src/casim.c", "    if (flags % IB_LIM / IB_LEVEL >= IB_WALKS || !room) return ib_one(rq, k, info, out, cap0, arena, cap);",
                             "    if (!room) return ib_one(rq, k, info, out, cap0, arena, cap);")],
    "steps-no-fallback": [("src/casim.c", "    if (ib_body(&K) != CASIM_OK) return ib_one(rq, k, info, out, cap0, arena, cap);",
                           "    if ((st = ib_body(&K)) != CASIM_OK) return st;"),
                          ("src/casim.c", "    if (flags % IB_LIM / IB_LEVEL >= IB_WALKS || !room) return ib_one(rq, k, info, out, cap0, arena, cap);",
                           "    if (flags % IB_LIM / IB_LEVEL >= IB_WALKS) return ib_one(rq, k, info, out, cap0, arena, cap);\n    if (!room) return CASIM_NO_ROOM;")],
    "steps-labels-not-free": [("src/casim.c", "        if (!used) return l;\n    }\n    for (int l = 25;", "        return l;\n    }\n    for (int l = 25;")],
    "steps-sub-note-unchecked": [("src/casim.c", "        if (c->st == CASIM_OK && is_k(c, z, 0, 1)) {",
                                  "        if (c->st == CASIM_OK && (is_k(c, z, 0, 1) || 1)) {")],
    "steps-u-not-whole": [("src/casim.c", "            if (wf != SPARE && !(sc.p == 1 && sc.q == 1)) {",
                           "            if (0) {")],
    "lnform-highest-negative": [("src/casim.c", "            r.p = (p[d].p < 0 ? -l : l) / h, r.q = g / h;",
                                 "            r.p = (p[d].p < 0 ? l : -l) / h, r.q = g / h;")],
    "lnform-abs-dropped": [("src/casim.c", "        none = none || ib_rootless(c, a);", "        none = 1 || ib_rootless(c, a);")],
    "lnform-rootless-exp-only": [("src/casim.c", "        else if (!ib_semi(c, y)) return false;", "        else if (!ib_semi(c, y) && 0) return false;")],
    # CAS 017's steps build, the cold review's seven findings
    "review-stale-note": [("src/casim.c", "    if (d == 0) B->how = IBH_NONE;\n    return ib_failr(c, B, mark);",
                           "    if (d == 0 && 0) B->how = IBH_NONE;\n    return ib_failr(c, B, mark);")],
    "review-whole-unbounded": [("src/casim.c", "           !__builtin_mul_overflow(ac, (int64_t)4, &ac4) && b2 < ac4;",
                                "           (ac4 = ac * 4, 1) && b2 < ac4;")],
    "review-cycle-div-bare": [("src/casim.c", "    bool b = s[0] == '-' || strpbrk(s, \"+-^(\") != NULL || strstr(s, TIMES) != NULL || strstr(s, \"\\xc3\\xb7\") != NULL;",
                               "    bool b = s[0] == '-' || strpbrk(s, \"+-\") != NULL;")],
    "review-sum-no-plus": [("src/casim.c", "                if (i) pf_puts(w, \"+\");", "                if (i && P->s[i][0] != '-') pf_puts(w, \"+\");")],
    "review-too-long-bad-arg": [("src/casim.c", "        if (st == CASIM_TOO_LONG) *tl = true;", "        if (st == CASIM_TOO_LONG && 0) *tl = true;")],
    "review-lnform-in-search": [("src/casim.c", "                    if (c->st == CASIM_OK && ib_same(c, mul2(c, q, du), t)) {",
                                 "                    if (c->st == CASIM_OK && ib_same(c, mul2(c, q, du), t)) {\n                        F = ib_canon(c, ib_lnform(c, F, var));")],
    "review-labels-exhausted": [("src/casim.c", "    for (int l = 25; l >= 0; l--) {                    /* the list taken: any letter free in text */",
                                 "    for (int l = 25; l >= 0 && 0; l--) {               /* the list taken: any letter free in text */")],
    "steps-walk-not-asked": [("src/casim.c", "    if ((st = ib_one(rq, -1, info, out, cap0, arena, cap)) != CASIM_OK) return st;",
                              "    if ((st = ib_one(rq, -1, info, out, cap0, arena, cap)) != CASIM_OK && 0) return st;")],
    # abacus's hold on the steps (#8227): never refused where integrate answers, at a page cap
    "hold-count-no-retry": [("src/casim.c",
                             "        if ((st != CASIM_NOT_INTEGRABLE && st != CASIM_NO_ROOM) || !cap) return st;",
                             "        return st;")],
    "hold-lines-unmeasured": [("src/casim.c",
                               "    if (lim) {                                          /* each own line, measured",
                               "    if (0) {                                          /* each own line, measured")],
    "hold-piece-no-cap": [("src/casim.c", "K.S.flags = flags % IB_LIM + IB_LEVEL + (int)lim * IB_LIM,",
                           "K.S.flags = flags % IB_LIM + IB_LEVEL,")],
    "hold-sum-answer-unmeasured": [("src/casim.c", "pf_puts(w, P->s[P->sum ? nt : 1]);", "pf_puts(w, P->s[1]);")],
    "hold-level-with-cap": [("src/casim.c", "    if (flags % IB_LIM / IB_LEVEL >= IB_WALKS || !room)",
                             "    if (flags / IB_LEVEL >= IB_WALKS || !room)")],
    # CAS 018: sums
    "sum-lo-sign": [("src/casim.c", "        sum = add2(c, sum, ib_neg(c, mul2(c, ib_at(c, F, var, lo)",
                     "        sum = add2(c, sum, (mul2(c, ib_at(c, F, var, lo)")],
    "sum-binom-start": [("src/casim.c", "            int64_t bin = i + 1;                                        /* C(i+1, i) */",
                         "            int64_t bin = 1;")],
    "sum-poly-check-skipped": [("src/casim.c", "            if (c->st == CASIM_OK && !ib_same(c, z, small(c, C_0)))\n",
                                "            if (c->st == CASIM_OK && !ib_same(c, z, small(c, C_0)) && 0)\n")],
    "sum-geom-check-skipped": [("src/casim.c", "            if (c->st == CASIM_OK && (F == SPARE || !ib_same(c, z, small(c, C_0))))",
                                "            if (c->st == CASIM_OK && F == SPARE)")],
    "sum-class-not-zero": [("src/casim.c", "        if (c->st == CASIM_OK && !is_k(c, ib_canon(c, tot), 0, 1)) return c->st = CASIM_NO_CLOSED_FORM, SPARE;",
                            "        if (0) return SPARE;")],
    "sum-pole-off": [("src/casim.c", "            if (s.q == 1 && sm_in(S, -s.p)) return c->st = CASIM_UNDEFINED, SPARE;     /* a pole in the range */",
                      "")],
    "sum-removed-rows-off": [("src/casim.c", "            if (q[0] != 'A' + var || q[1] != '=') continue;           /* \"p=0\": no rational root */",
                              "            continue;")],
    "sum-const-dropped": [("src/casim.c", "P, coef, B, f = 0, cf = k;", "P, coef, B, f = 0, cf = small(c, C_1);")],
    "sum-hole-rule-off": [("src/casim.c", "        if (r == 0 && !sm_typed_ok(&c, S.raw, S.f, var) && c.st == CASIM_OK) c.st",
                           "        if (r == 0 && !sm_typed_ok(&c, S.raw, S.f, var) && c.st == CASIM_OK && 0) c.st")],
    "sum-positive-base-hole": [("src/casim.c", "        if (!contains(c, b, var) && const_sign(c, b) > 0) continue;\n", "")],
    "sum-empty-dropped": [("src/casim.c", "        if (S.ilo && S.ihi && S.b < S.a) return finish(&c, small(&c, C_0), out, ocap);   /* rule 2: none */",
                           "")],
    "sum-direct-65": [("src/casim.c", "(uint64_t)S.b - (uint64_t)S.a < SM_DIRECT && !closed)", "(uint64_t)S.b - (uint64_t)S.a <= SM_DIRECT && !closed)")],
    "sum-conv-le-1": [("src/casim.c", "            if (s < 0) continue;", "            if (s < 0 || s == 2) continue;")],
    "sum-smaller-dominates": [("src/casim.c", "            } else if (s > 0) best = gi, tie = false;", "            } else if (s < 0) best = gi, tie = false;")],
    "sum-sign-flipped": [("src/casim.c", "        *dir = sg;", "        *dir = -sg;")],
    "sum-swing-as-inf": [("src/casim.c", "        if (g->B && const_sign(c, g->B) < 0) return c->st = CASIM_UNDEFINED, SPARE;      /* it swings: no limit */",
                          "")],
    "sum-tie-ignored": [("src/casim.c", "        if (tie) return c->st = CASIM_UNDECIDED, SPARE;",
                         "        if (tie && 0) return c->st = CASIM_UNDECIDED, SPARE;")],
    "sum-symbolic-lo-rational": [("src/casim.c", "        if (c.st == CASIM_OK && nr && !S.ilo) c.st = CASIM_NO_CLOSED_FORM;        /* rule 2: it would divide by lo */",
                                  "")],
    # CAS 018's cold review
    "sum-literal-bound": [("src/casim.c", "(!letter && !*whole) || sm_lit(c, r)) c->st = CASIM_BAD_ARG;",
                           "(!letter && !*whole)) c->st = CASIM_BAD_ARG;")],
    "sum-pf-text-kept": [("src/casim.c", "        if (c->st == CASIM_OK && sm_lit(c, P)) c->st = CASIM_NO_ROOM;     /* its text is partfrac's, not eqn */\n        c->s = S->eqn;\n",
                          "        if (c->st == CASIM_OK && sm_lit(c, P)) c->st = CASIM_NO_ROOM;     /* its text is partfrac's, not eqn */\n")],
    "sum-ratio-one": [("src/casim.c", "            if (w && !sm_letter(c, B) && const_sign(c, ib_canon(c, add2(c, B, small(c, C_M1)))) == 0)",
                       "            if (w && !sm_letter(c, B) && const_sign(c, ib_canon(c, add2(c, B, small(c, C_M1)))) == 0 && 0)")],
    "sum-base-zero": [("src/casim.c", "        if (base && const_sign(c, base) == 0 && !sm_letter(c, base)) {",
                       "        if (base && const_sign(c, base) == 0 && !sm_letter(c, base) && 0) {")],
    "sum-poly-not-faster": [("src/casim.c", "                int e = g->d + !B, eb = G[best].d + !G[best].B;", "                int e = g->d, eb = G[best].d;")],
    "sum-swing-before-sign": [("src/casim.c", "        if (tie) return c->st = CASIM_UNDECIDED, SPARE;\n",
                               "        if (tie) return c->st = CASIM_UNDECIDED, SPARE;\n        if (g->B && const_sign(c, g->B) < 0) return c->st = CASIM_UNDEFINED, SPARE;\n")],
    "sum-symbolic-empty": [("src/casim.c", "val(&c, span)->q == 1 && val(&c, span)->p < 0)", "val(&c, span)->q == 1 && val(&c, span)->p < 0 && 0)")],
    "sum-exp-divisor": [("src/casim.c", "(nd(c, y->a)->id == F_EXP || nd(c, y->a)->id == F_ALOG)) {", "(nd(c, y->a)->id == F_EXP || nd(c, y->a)->id == F_ALOG) && 0) {")],
    "sum-print-word": [("src/casim.c", "    if (!buf && c->st == CASIM_NOT_INTEGRABLE) c->st = CASIM_NO_ROOM;", "    if (0) c->st = CASIM_NO_ROOM;")],
    "sum-row-read-late": [("src/casim.c", "                v = ib_canon(c, v);                             /* a raw number reads its digits from c->s */\n                c->s = S->eqn;\n",
                           "                c->s = S->eqn;\n                v = ib_canon(c, v);\n")],
    # CAS 018 rule 4b: a letter in the ratio, with its rows
    "sum-4b-even-root": [("src/casim.c", "        sm_row(c, rows, n, leaf(c, K_VAR, L), '!', ib_canon(c, ib_neg(c, u)), L);\n", "")],
    "sum-4b-inf-op": [("src/casim.c", "m > 0 ? '<' : '>',", "'<',")],
    "sum-4b-inf-power": [("src/casim.c", "            rat ak = k, e = {-1, m};", "            rat ak = k, e = {1, m};")],
    "sum-4b-row-order": [("src/casim.c", "    for (at = *n; at > 0; at--) {", "    for (at = *n; at > 0 && 0; at--) {")],
    "sum-4b-row-twice": [("src/casim.c", "        if (rows[i].op == op && cmp(c, rows[i].l, l) == 0 && cmp(c, rows[i].r, r) == 0) return;      /* once */",
                          "        if (0) return;")],
    "sum-4b-swing-proven": [("src/casim.c", "        if (letter) c.st = CASIM_UNDECIDED;", "        if (letter && 0) c.st = CASIM_UNDECIDED;")],
    "sum-4b-letter-ranked": [("src/casim.c", "        if (!S->inf || (B && sm_letter(c, B))) continue;", "        if (!S->inf) continue;")],
    "sum-4b-closed-first": [("src/casim.c", "    casim_status st = do_sum_(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, rtl, false, &direct);",
                             "    casim_status st = do_sum_(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, rtl, true, &direct);")],
    "sum-4b-no-fallback": [("src/casim.c", "    if (st != CASIM_TOO_LONG || !direct) return st;", "    if (st != CASIM_TOO_LONG || !direct || 1) return st;")],
    "sum-4b-odd-sign": [("src/casim.c", "'!', t.p < 0 ? ib_canon(c, ib_neg(c, u)) : u, L);", "'!', u, L);")],
    # CAS 018 after h018 (abacus #8460): a typed n÷d's terms are one rational term; equal fractions one entry
    # abacus #8494: the divisors compared in sm_put keep their room
    "sum-closed-room-as-long": [("src/casim.c", "    return st == CASIM_OK || st == CASIM_NO_ROOM ? st : CASIM_TOO_LONG;",
                                 "    return st == CASIM_OK ? st : CASIM_TOO_LONG;")],
    "sum-put-room-kept": [("src/casim.c", "        taylor_drop(c, mark);\n        if (eq) break;", "        (void)mark;\n        if (eq) break;")],
    "sum-terms-unmerged": [("src/casim.c", "    if (k < *nr) R[k] = ib_canon(c, add2(c, R[k], t));", "    if (0) R[k] = ib_canon(c, add2(c, R[k], t));")],
    "sum-fractions-unmerged": [("src/casim.c", "                if (at < nt) tm[at].cf = ib_canon(c, add2(c, tm[at].cf, cm));",
                                "                if (0) tm[at].cf = ib_canon(c, add2(c, tm[at].cf, cm));")],
    # CAS 018 rule 4b's shift (abacus #8349): a negative power of the ratio's letter cleared
    "sum-shift-off": [("src/casim.c", "    if (s == 0 || s > SM_DEG || s < -SM_DEG) return 0;\n    Bs =",
                       "    if (1) return 0;\n    Bs =")],
    "sum-shift-bounds": [("src/casim.c", "        lo = sh ? add2(c, S->lo, sm_int(c, -sh)) : S->lo;", "        lo = S->lo;")],
    "sum-shift-binom": [("src/casim.c", "            for (int r = 1; r <= j - i; r++) w /= r;", "")],
    # CAS 018 rule 4b's cold review
    "sum-4b-no-hole-row": [("src/casim.c", "            hole = c->st == CASIM_UNDEFINED;", "            hole = false;")],
    "sum-4b-pole-as-swing": [("src/casim.c", "    swung = c.st == CASIM_OK;", "    swung = true;")],
    # abacus #8359: a search that ran out is NO_ROOM
    "noroom-ranout-ignored": [("src/casim.c", "c.st = B.ranout ? CASIM_NO_ROOM : CASIM_NOT_INTEGRABLE;",
                               "c.st = CASIM_NOT_INTEGRABLE;")],
    "noroom-try-unnoted": [("src/casim.c", "    if (c->st == CASIM_NO_ROOM) B->ranout = true;\n    return ib_fail(c, mark);",
                            "    (void)B;\n    return ib_fail(c, mark);")],
    "noroom-rule8-searched": [("src/casim.c", "    if (ir_cancels(eqn, var, angle, arena, arena_cap, rtl)) return CASIM_NOT_INTEGRABLE;",
                               "    if (ir_cancels(eqn, var, angle, arena, arena_cap, rtl) && 0) return CASIM_NOT_INTEGRABLE;")],
    "noroom-rule8-steps": [("src/casim.c", "    if (st == CASIM_NOT_INTEGRABLE && ir_cancels(rq->eqn, var, rq->angle, arena, arena_cap, rtl)) return st;",
                            "    if (st == CASIM_NOT_INTEGRABLE && ir_cancels(rq->eqn, var, rq->angle, arena, arena_cap, rtl) && 0) return st;")],
    # CAS 013 rule 8 amended (abacus #8494, #8510): the reduced function, before the search, in integrate and the steps
    "rule8-reduced-off": [("src/casim.c", "    if ((r = ir_reduced(eqn, var, angle, out, cap, arena, arena_cap, rtl)) == CASIM_OK) return r;",
                           "    r = CASIM_NOT_INTEGRABLE;\n    (void)ir_reduced;")],
    "rule8-reduced-after-search": [("src/casim.c",
        "    if ((r = ir_reduced(eqn, var, angle, out, cap, arena, arena_cap, rtl)) == CASIM_OK) return r;\n    st = do_iparts(eqn, var, angle, out, cap, arena, arena_cap, rtl);\n",
        "    st = do_iparts(eqn, var, angle, out, cap, arena, arena_cap, rtl);\n    if (st == CASIM_OK || (r = ir_reduced(eqn, var, angle, out, cap, arena, arena_cap, rtl)) == CASIM_OK) return st == CASIM_OK ? st : r;\n")],
    "rule8-steps-unreduced": [("src/casim.c", "    const casim_req *g = irs_reduce(rq, tc);", "    const casim_req *g = NULL;\n    (void)irs_reduce;")],
    "rule8-steps-hole-reduced": [("src/casim.c", "        !(rows = strchr(txt, ';')) || ir_cancels(rq->eqn, rq->var, rq->angle, tc->mem, room, rtl))",
                                  "        !(rows = strchr(txt, ';')))")],
    "rule8-root-names-reduced": [("src/casim.c", "    if (want == 3) mid = rq->repl;\n", "")],
    # abacus #8413: a rational piece with no room for partfrac to start is a run-out
    "noroom-start-unnoted": [("src/casim.c", "&& ib_ratfn(c, u, var)) st = CASIM_NO_ROOM;", "&& ib_ratfn(c, u, var) && 0) st = CASIM_NO_ROOM;")],
    "noroom-ratfn-any": [("src/casim.c", "    if (x->kind == K_NUM) return true;\n    if (x->kind == K_VAR) return x->id == var;",
                          "    if (x->kind == K_NUM || 1) return true;\n    if (x->kind == K_VAR) return x->id == var;")],
    # abacus #8423: a removed factor is a hole only at a real root
    "holes-no-real-root-counted": [("src/casim.c", "           strncmp(slot - SLOT, \"NO REAL ROOTS\", 13) != 0;", "           1;")],
    "noroom-rational-unnoted": [("src/casim.c", "            if (st == CASIM_NO_ROOM) B->ranout = true;\n",
                                 "            (void)B;\n")],
    "review2-replay-to-parts": [("src/casim.c", "        ib_fail(c, mark);                       /* the search's substitution, not replayed here (the arena): one page, */\n        return;",
                                 "        ib_fail(c, mark);                       /* the search's substitution, not replayed here (the arena): one page, */")],
    "divisor-bases-from-d": [("src/casim.c", "    ref dc = ib_inv(c, ib_canon(c, d));\n    const node *x = nd(c, dc);",
                              "    ref dc = ib_canon(c, d);\n    const node *x = nd(c, dc);")],
    "const-left-in-rational": [("src/casim.c", "        if (!is_num(c, f) && !contains(c, f, var)) k = k ? mul2(c, k, f) : f;",
                                "        if (!is_num(c, f) && !contains(c, f, var) && 0) k = k ? mul2(c, k, f) : f;")],
    "const-on-whole-answer": [("src/casim.c", "            if (k && nd(c, F)->kind == K_ADD) {", "            if (0) {")],
    "letter-rule-off": [("src/casim.c", "                ib_letters_kept(c, g, U, t)) {", "                (ib_letters_kept(c, g, U, t) || 1)) {"),
                        ("src/casim.c",
                         "        if (c.st == CASIM_OK && !ib_letters_kept(&c, F, var, t)) c.st = CASIM_NOT_INTEGRABLE;",
                         "        if (c.st == CASIM_OK && !ib_letters_kept(&c, F, var, t) && 0) c.st = CASIM_NOT_INTEGRABLE;")],
    "back-sub-k-not-1-over-k": [("src/casim.c", "                                  power(c, nd(c, xk)->b, small(c, C_M1))));",
                                 "                                  nd(c, xk)->b));")],
    "liate-ln-last": [("src/casim.c", "    case F_LN: case F_LOG: return 5;", "    case F_LN: case F_LOG: return 1;")],
    "ib-depth-short": [("src/casim.c", "    if (d >= IB_DEPTH) return c->st = CASIM_NOT_INTEGRABLE, SPARE;",
                        "    if (d >= IB_DEPTH - 1) return c->st = CASIM_NOT_INTEGRABLE, SPARE;")],
    "ib-steps-refuse": [("src/casim.c",
                         "if (st != CASIM_OK) return st == CASIM_NOT_INTEGRABLE ? (info->line = flags, ib_walk(rq, k, info, out, cap, arena, arena_cap)) : st;",
                         "if (st != CASIM_OK) return st == CASIM_NOT_INTEGRABLE && 0 ? (info->line = flags, ib_walk(rq, k, info, out, cap, arena, arena_cap)) : st;")],
    "ib-constants-kept": [("src/casim.c", "        if (c.st == CASIM_OK) F = ib_lnfinal(&c, ib_drop(&c, F, var), var);\n        if (c.st == CASIM_OK && !ib_letters_kept", "        if (c.st == CASIM_OK) F = ib_lnfinal(&c, F, var);\n        if (c.st == CASIM_OK && !ib_letters_kept")],
    "ib-tries-few": [("src/casim.c", "#define IB_TRIES 160", "#define IB_TRIES 4")],
    "ib-bad-off": [("src/casim.c", "    if (++B->tries > IB_TRIES || ib_bad(c, t, var)) return",
                    "    if (++B->tries > IB_TRIES || (ib_bad(c, t, var) && 0)) return")],
    "ib-domain-rule-off": [("src/casim.c", "    if (ok) ok = ib_domain_kept(c, r, t, var);",
                            "    if (ok && 0) ok = ib_domain_kept(c, r, t, var);")],
    "ib-letters-guard-off": [("src/casim.c",
                              "        if (c.st == CASIM_OK && !ib_letters_kept(&c, F, var, t)) c.st = CASIM_NOT_INTEGRABLE;",
                              "        if (c.st == CASIM_OK && !ib_letters_kept(&c, F, var, t) && 0) c.st = CASIM_NOT_INTEGRABLE;")],
    "ib-deep-no-room-kept": [("src/casim.c", "    c->st = CASIM_OK;\n    c->starved = false;\n    taylor_drop(c, mark);",
                              "    if (c->st == CASIM_NO_ROOM) return false;\n    c->st = CASIM_OK;\n    c->starved = false;\n    taylor_drop(c, mark);")],
    # CAS 019 (fractions, abacus #8547/#8557): the mark-dropping family, each fault at one site, and the print rule's
    "mark-parse-none": [("src/casim.c", "    return points == 1 || (points == 0 && e);", "    return (points == 1 || (points == 0 && e)) && 0;")],
    "mark-parse-abc": [("src/casim.c", "    return points == 1 || (points == 0 && e);", "    return points >= 1 || (points == 0 && e);")],
    "mark-fold-dropped": [("src/casim.c", "        if (marked(c, t[i])) c->nm |= NM_FOLD;", "        if (marked(c, t[i]) && 0) c->nm |= NM_FOLD;")],
    "mark-run-dropped": [("src/casim.c", "        for (int q = i; q < j; q++) rm |= coef_mark(c, t[q]);",
                          "        for (int q = i; q < j && 0; q++) rm |= coef_mark(c, t[q]);")],
    "mark-sum-constant": [("src/casim.c", "    if (!rat_is(&sum, 0, 1)) t[k++] = numm(c, &sum, sm);",
                           "    if (!rat_is(&sum, 0, 1)) t[k++] = numm(c, &sum, sm && 0);")],
    "mark-content-dropped": [("src/casim.c", "    km = content_mark(c, *f, &k);", "    km = false;\n    (void)content_mark;")],
    "mark-sumdiv-term": [("src/casim.c", "            r = numm(c, &q, km || marked(c, t));", "            r = numm(c, &q, km);")],
    "mark-mul-coefficient": [("src/casim.c", "marked(c, reuse) != cm ? numm(c, &cv, cm) : reuse;", "marked(c, reuse) != cm ? num(c, &cv) : reuse;")],
    "mark-power-dropped": [("src/casim.c", "    return numm(c, p, marked(c, b) || marked(c, e));", "    return numm(c, p, ((void)b, marked(c, e)));")],
    "mark-power-sum": [("src/casim.c", "    bool km = content_mark(c, b, &k) || marked(c, e);", "    bool km = marked(c, e);")],
    "mark-dedupe-unmarked-base": [("src/casim.c", "&& marked(c, base_of(c, t[q]))) return numm(c, val(c, b), true);",
                                   "&& marked(c, base_of(c, t[q]))) return b;")],
    "mark-abs-dropped": [("src/casim.c", "            return numm(c, &v, marked(c, a));", "            return num(c, &v);")],
    "mark-logs-dropped": [("src/casim.c", "                    gm |= coef_mark(c, t[j]) || marked(c, nd(c, lf)->a);",
                           "                    gm |= false && (coef_mark(c, t[j]) || marked(c, nd(c, lf)->a));")],
    "mark-exact-trig": [("src/casim.c", "        if (table_val(f, n, &v)) return xval_tree(c, &v, any_marked(c, a));",
                         "        if (table_val(f, n, &v)) return xval_tree(c, &v, false);")],
    "mark-pw-literal": [("src/casim.c", "        if (lit_marked(lit_text(c, x), x->n)) c->nm |= NM_PW;",
                         "        if (lit_marked(lit_text(c, x), x->n) && 0) c->nm |= NM_PW;")],
    "mark-pw-term-kept": [("src/casim.c", "    c->nm &= (uint8_t)~NM_PW;           /* CAS 019 rule 3: the table's numbers, one mark a typed term */",
                           "")],
    "mark-pnum-dropped": [("src/casim.c", "    return numm(c, v, (c->nm & NM_PW) != 0);", "    return numm(c, v, false);")],
    "mark-rational-text": [("src/casim.c", "    if (text_marked(eqn)) c.nm |= NM_PW;", "    if (text_marked(eqn) && 0) c.nm |= NM_PW;")],
    "mark-point-quotient": [("src/casim.c", "    put_int(w, a.q);\n    if (pt) puts_(w, \".\");\n    return 2;", "    put_int(w, a.q);\n    return 2;")],
    "mark-point-coefficient": [("src/casim.c", "        if (f0 && pt_of(c, f0)) puts_(w, \".\");", "")],
    "mark-point-whole": [("src/casim.c", "!memchr(w->o + n0, 'E', w->n - n0))\n        puts_(w, \".\");", "!memchr(w->o + n0, 'E', w->n - n0) && 0)\n        puts_(w, \".\");")],
    "print-ignores-mark": [("src/casim.c", "    return (c->nm & NM_DEC) || marked(c, r);", "    return (void)r, (c->nm & NM_DEC) != 0;")],
    "print-all-decimal": [("src/casim.c", "    return (c->nm & NM_DEC) || marked(c, r);", "    return true || marked(c, r);")],
    "unpoint-skipped": [("src/casim.c", "    st = unpoint(st, out, cap);\n    return fell", "    return fell")],
    # (dec-not-nested, the nested rational path called without →DEC's bit, survives: its text is read back as a tree,
    # so the bit changes only the text's length against IB_TEXT, and no rational part the by-parts path integrates
    # comes near 384 B (1,600 random EXP/SIN/X×EXP plus p÷q cases, and degree-6 ones it refuses). The bit stays, so a
    # →DEC walk's inner text is the old printer's; the mutant is equivalent on everything reached.)
    "fallback-unflagged": [("src/casim.c", "    return fell && st == CASIM_OK && cap && has_decimal(out) ? CASIM_DECIMAL_FORM : st;",
                            "    return (void)has_decimal, (void)fell, st;")],
    "fallback-skipped": [("src/casim.c", "    if ((st == CASIM_TOO_LONG || st == CASIM_NO_ROOM) && !(rq->angle & ANGLE_DEC))",
                          "    if ((st == CASIM_TOO_LONG || st == CASIM_NO_ROOM) && !(rq->angle & ANGLE_DEC) && 0)")],
    "fallback-any-status": [("src/casim.c", "    if ((st == CASIM_TOO_LONG || st == CASIM_NO_ROOM) && !(rq->angle & ANGLE_DEC))",
                             "    if (st != CASIM_OK && !(rq->angle & ANGLE_DEC))")],
    # rule 5 as amended (abacus #8764): a fallback answer with no decimal is OK; both failing, the fallback's status
    "fallback-flags-no-decimal": [("src/casim.c", "cap && has_decimal(out) ? CASIM_DECIMAL_FORM", "cap && (has_decimal(out) || 1) ? CASIM_DECIMAL_FORM")],
    "fallback-e-not-decimal": [("src/casim.c", "        if (*t == '.' || (*t == 'E' && t > s && digit(t[-1]))) return true;",
                                "        if (*t == '.') return true;")],
    "fallback-first-status": [("src/casim.c", "        fell = true, st = run_(rq, rq->angle | ANGLE_DEC, out, cap, arena, arena_cap);",
                               "        fell = run_(rq, rq->angle | ANGLE_DEC, out, cap, arena, arena_cap) == CASIM_OK, st = fell ? CASIM_OK : st;")],
    # CAS 020 (complex roots): the pair, its text, its order, and where the bit may go
    "cx-pair-sign-dropped": [("src/casim.c", "iroot x = {d < 0 ? 2 : 1, 0, 1, -b / g, s * (k / g), r,",
                              "iroot x = {d < 0 ? 2 : 1, 0, 1, -b / g, (d < 0 ? 1 : s) * (k / g), r,")],
    "cx-i-left-out": [("src/casim.c", "        if (x->irr == 2) puts_(w, \"0i1\");", "        if (x->irr == 2 && 0) puts_(w, \"0i1\");")],
    "cx-bare-i": [("src/casim.c", "        if (x->irr == 2) puts_(w, \"0i1\");", "        if (x->irr == 2) puts_(w, \"i\");")],
    "cx-parts-unreduced": [("src/casim.c", "    g = gcd64(gcd64(b, k), 2 * a);\n    for (int s = -1; s <= 1; s += 2) {",
                            "    g = d < 0 ? 1 : gcd64(gcd64(b, k), 2 * a);\n    for (int s = -1; s <= 1; s += 2) {")],
    "cx-r-not-square-free": [("src/casim.c", "    square_free(d < 0 ? -d : d, &k, &r);",
                              "    if (d < 0) k = 1, r = -d;\n    else square_free(d, &k, &r);")],
    "cx-pairs-by-im-descending": [("src/casim.c", "    if ((k = abn_cmp(&x, &y)) != 0) return k > 0;\n    return a->B > b->B;",
                                   "    if ((k = abn_cmp(&x, &y)) != 0) return k < 0;\n    return a->B > b->B;")],
    "cx-negative-last": [("src/casim.c", "    return a->B > b->B;\n}", "    return a->B < b->B;\n}")],
    "cx-pairs-before-reals": [("src/casim.c", "    if ((a->irr == 2) != (b->irr == 2)) return a->irr == 2;",
                               "    if ((a->irr == 2) != (b->irr == 2)) return b->irr == 2;")],
    "cx-steps-ignore-bit": [("src/casim.c", "    if (solve_flags(rq->model, rq->angle) & SF_CX) c.nm |= NM_CX;     /* the walk's quadratics as solve's */",
                             "    (void)0;")],
    # the core's own literal AiB for r = 1 (abacus #9364): a leading minus negates the whole literal, so -1+2i reads -1-2i
    "cx-literal-negative-real": [("src/casim.c", "        int64_t b = x->B < 0 ? -x->B : x->B;\n        bool paren = x->A != 0 && x->D != 1;",
                                  "        int64_t b = x->B < 0 ? -x->B : x->B;\n        bool paren = x->A != 0 && x->D != 1;\n"
                                  "        if (x->irr == 2 && x->r == 1) {\n            put_int(w, x->A), puts_(w, \"i\"), put_int(w, x->B);\n"
                                  "            if (x->D != 1) puts_(w, DIVIDE), put_int(w, x->D);\n"
                                  "            if (x->mult > 1) puts_(w, \" (MULT \"), put_int(w, x->mult), puts_(w, \")\");\n            return;\n        }")],
    "cx-no-real-roots-with-bit": [("src/casim.c", "    } else puts_(w, nr ? \"SOLVED\" : \"NO REAL ROOTS\");",
                                   "    } else puts_(w, nr && rs[0].irr != 2 ? \"SOLVED\" : \"NO REAL ROOTS\");")],
    "cx-bit-in-33": [("src/casim.c", "((angle & ANGLE_CX) && model != CASIM_MODEL_33 ? SF_CX : 0)", "((angle & ANGLE_CX) ? SF_CX : 0)")],
    "cx-boundary-ir-hole": [("src/casim.c", "arena_cap - 3 * SLOT, rtl ? SF_RTL : 0) != CASIM_OK ||",
                             "arena_cap - 3 * SLOT, (rtl ? SF_RTL : 0) | SF_CX) != CASIM_OK ||")],
    "cx-start-keeps-bit": [("src/casim.c", "    c->angle = angle & ~ANGLE_BITS;", "    c->angle = angle & ~ANGLE_DEC;")],
    "collect-fold-off": [("src/casim.c", "    } else if (t->k > 0 && !(c->nm & NM_DEC)) {", "    } else if (t->k > 0 && !(c->nm & NM_DEC) && 0) {")],
    "collect-fold-under-dec": [("src/casim.c", "    } else if (t->k > 0 && !(c->nm & NM_DEC)) {", "    } else if (t->k > 0) {")],
    "mark-cache-poisoned": [("src/casim.c", "    ref r = num(c, v);\n    if (m && r != SPARE) nd(c, r)->id = NUM_MARK;",
                             "    ref r = rat_is(v, 1, 2) ? small(c, C_HALF) : num(c, v);\n    if (m && r != SPARE) nd(c, r)->id = NUM_MARK;")],
    # CAS 021 (series, work/021-series.md's list): a sign read wrong, a side skipped, precision not retried, an exact
    # series as inexact, a give-up that answers, the boundary, an UNDEFINED undecided, the zero test, LN's give-up,
    # taylor's poles, and rule 7's own run-out
    "ser-abs-sign-flipped": [("src/casim.c", "        return sg > 0 ? U : s_scale(s, U, small(c, C_M1));",
                              "        return sg < 0 ? U : s_scale(s, U, small(c, C_M1));")],
    "ser-taylor-one-side": [("src/casim.c", "        if (u > 0 && s.sided) {", "        if (u > 0 && s.sided && 0) {")],
    "ser-limit-one-side": [("src/casim.c", "    dn = side <= 0 ? limit_side(c, f, a, -1) : up;", "    dn = up;")],
    "ser-taylor-no-retry": [("src/casim.c", "        if (u == 0) {\n            taylor_drop(c, base);\n            continue;\n        }",
                             "        if (u == 0) {\n            (void)base;\n            break;\n        }")],
    "ser-limit-no-retry": [("src/casim.c", "        taylor_drop(c, base);           /* too few terms known: more */",
                            "        (void)base;\n        break;")],
    "ser-exact-as-inexact": [("src/casim.c", "static bool s_exact(const ctx *c, ref r) { return s_zero(c, r) || nd(c, r)->b == 1; }",
                              "static bool s_exact(const ctx *c, ref r) { return s_zero(c, r); }")],
    "ser-puiseux-answers": [("src/casim.c", "    if (((long long)s_v(c, x) * al->p) % al->q) return s->fail = true, 0;", "")],
    "ser-root-of-negative": [("src/casim.c", "        if (sg <= 0) return s->fail = true, 0;", "        if (sg == 0) return s->fail = true, 0;")],
    "ser-divisor-undecided": [("src/casim.c", "    } else if (al->p < 0 && sg == 0 && !is_num(c, s_cs(c, x)[0])) return s->fail = true, 0;",
                               "    }")],
    "ser-taylor-on-answered": [("src/casim.c", "    if (CASIM_SER && ocap && (c.st == CASIM_NO_ROOM || c.st == CASIM_UNDECIDED)) {",
                                "    if (CASIM_SER && ocap && (c.st == CASIM_NO_ROOM || c.st == CASIM_UNDECIDED || c.st == CASIM_OK)) {")],
    "ser-limit-on-answered": [("src/casim.c", "(c.st == CASIM_NO_ROOM || (c.st == CASIM_OK && r.k == L_UND))",
                               "(c.st == CASIM_NO_ROOM || c.st == CASIM_OK)")],
    "ser-undefined-undecided": [("src/casim.c", "            if (!is_k(c, d, 0, 1)) return s_sign(&s, d) ? CASIM_UNDEFINED : was;",
                                 "            if (!is_k(c, d, 0, 1)) return CASIM_UNDEFINED;")],
    "ser-limit-none-undecided": [("src/casim.c", "        return s_sign(&z, ib_canon(c, add2(c, up.v, neg(c, dn.v)))) ? lm(L_NONE, 0) : lm(L_UND, 0);",
                                  "        (void)z;\n        return lm(L_NONE, 0);")],
    # (CAS 022 made const_sign rule 8, so this mutant's 021 text, s_sign as const_sign, became a depth change alone;
    # it now puts CAS 009's 1E-20 test, what const_sign was at 021, in s_sign itself)
    "ser-zero-plain-value": [("src/casim.c", "    if (s_eval(c, k, &v, &b, 0) != CV_OK || !s_lt(&b, &v, 8)) return 0;\n    return abn_is_negative(&v) ? -1 : 1;",
                              "    abn tiny;\n    if (const_val(c, k, &v) != CV_OK || abn_pow10(&tiny, -20) != ABN_OK) return 0;\n"
                              "    b = abn_abs(&v);\n    if (abn_cmp(&b, &tiny) <= 0) return 0;\n    return abn_is_negative(&v) ? -1 : 1;")],
    "ser-bound-product-unscaled": [("src/casim.c", "                if (!s_m(&p1, &w, &tb) || !s_m(&p2, &tv, b) || !s_m(&p3, b, &tb) || abn_add(b, &p1, &p2) != ABN_OK ||",
                                    "                if (!s_m(&p1, &w, &tb) || !s_m(&p2, &w, &w) || !s_m(&p3, b, &tb) || abn_add(b, b, &tb) != ABN_OK ||")],
    "ser-ln-no-value": [("src/casim.c", "    if (domain && const_val(c, r, &t) != CV_OK) return s->fail = true, 0;", "    (void)t, (void)domain;")],
    "ser-after-rule7-ran-out": [("src/casim.c", "#define S_ZERO 255\n", "#define S_ZERO 255\nstatic int r7_ran_out;\n"),
                                ("src/casim.c", "        taylor_check(&c, raw, var, a);\n        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);",
                                 "        taylor_check(&c, raw, var, a);\n        if ((r7_ran_out = c.st == CASIM_NO_ROOM)) c.st = CASIM_OK;\n"
                                 "        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);"),
                                ("src/casim.c", "    if (CASIM_SER == 2 && c.st == CASIM_OK) c.st = CASIM_UNDECIDED;     /* the tests' build: the series route alone */",
                                 "    if (CASIM_SER == 2 && c.st == CASIM_OK) c.st = CASIM_UNDECIDED;\n    if (r7_ran_out && c.st == CASIM_OK) c.st = CASIM_NO_ROOM;")],
    "ser-limit-sign-of-inf": [("src/casim.c", "            if ((sg = s_sign(&s, s_cs(c, R)[0])) != 0) return lm(sg > 0 ? L_PINF : L_NINF, 0);",
                               "            if ((sg = s_sign(&s, s_cs(c, R)[0])) != 0) return lm(L_PINF, 0);")],
    "ser-inf-not-inverted": [("src/casim.c", "c->sub_val = power(c, leaf(c, K_VAR, c->var), small(c, C_M1));",
                              "c->sub_val = leaf(c, K_VAR, c->var);")],
    # CAS 022: signs decided by value (work/022-signs.md)
    "sign-lmsgn-exact": [("src/casim.c", "    if (s_eval(c, v, &x, &b, S_DEEP) != CV_OK || !s_lt(&b, &x, 8)) return 2;\n    return abn_is_negative(&x) ? -1 : 1;",
                          "    (void)b;\n    return abn_is_zero(&x) ? 0 : abn_is_negative(&x) ? -1 : 1;")],
    "sign-strict-reverted": [("src/casim.c", "    if (is_num(c, v) || is_k(c, v, 0, 1)) return abn_is_zero(&x) ? 0 : abn_is_negative(&x) ? -1 : 1;",
                              "    if (is_num(c, v) || is_k(c, v, 0, 1) || abn_is_zero(&x)) return abn_is_zero(&x) ? 0 : abn_is_negative(&x) ? -1 : 1;")],
    "sign-const-1e20": [("src/casim.c", "    if (s_eval(c, a, &v, &b, S_DEEP) != CV_OK || !s_lt(&b, &v, 8)) return 0;\n    return abn_is_negative(&v) ? -1 : 1;\n}",
                         "    abn tiny;\n    (void)b;\n    if (const_val(c, a, &v) != CV_OK || abn_pow10(&tiny, -20) != ABN_OK) return 0;\n    b = abn_abs(&v);\n    if (abn_cmp(&b, &tiny) <= 0) return 0;\n    return abn_is_negative(&v) ? -1 : 1;\n}")],
    "sign-hole-value": [("src/casim.c", "(cv == CV_OK && (s_eval(c, u, &x, &b, 0) != CV_OK || (!abn_is_zero(&x) && !s_lt(&b, &x, 8)))))",
                         "(cv == CV_OK && 0 && (s_eval(c, u, &x, &b, 0) != CV_OK || (!abn_is_zero(&x) && !s_lt(&b, &x, 8)))))")],
    "sign-hole-depth-deep": [("src/casim.c", "(cv == CV_OK && (s_eval(c, u, &x, &b, 0) != CV_OK",
                              "(cv == CV_OK && (s_eval(c, u, &x, &b, S_DEEP) != CV_OK")],
    "sign-hole-form": [("src/casim.c", "            if (!h && !any_letter(c, u) && !const_sign(c, u)) c->st = CASIM_UNDECIDED;",
                        "            (void)0;")],
    # (taylor_zero's undecided test is left out, as unreachable: taylor_hole's SGN test refuses first, and the corner
    # path's const_sign refuses after it; work/022-signs.md "As built". Its guard stays.)
    "sign-range-notreal": [("src/casim.c", "           st == ABN_OVERFLOW || st == ABN_UNDERFLOW ? CV_NO : CV_NOTREAL;",
                            "           CV_NOTREAL;")],
    "sign-range-decides": [("src/casim.c", "            if (cv == CV_NO || (cv == CV_OK && (s_eval(c, u, &x, &b, 0)",
                            "            if ((cv == CV_OK && (s_eval(c, u, &x, &b, 0)")],
    "sign-sides-form": [("src/casim.c", "                r = sd == 0 ? up : sd == 2 ? lm(L_UND, 0) : lm(L_NONE, 0);",
                         "                r = ((void)sd, lm(L_NONE, 0));")],
    "sign-sides-letters": [("src/casim.c", "                int sd = any_letter(&c, up.v) || any_letter(&c, dn.v) ? 1",
                            "                int sd = 0 ? 1")],
    "sign-value-drift": [("src/casim.c", "        *v = w;                                         /* const_val's sum or product, in its order */",
                          "        *v = w;                                         /* const_val's sum or product, in its order */\n"
                          "        if (x->kind == K_ADD) { abn k1, o1 = abn_from_int(1); abn_pow10(&k1, -33), abn_add(&k1, &k1, &o1), abn_mul(v, v, &k1); }")],
    "sign-fold-abs-undecided": [("src/casim.c", "            if (sg) return sg > 0 ? a : neg(c, a);",
                                 "            if (!sg && !any_letter(c, a)) sg = 1;\n            if (sg) return sg > 0 ? a : neg(c, a);")],
    # 022's follow-up (abacus #9481/#9482): lm_quot divides by a residue again (L'Hôpital's 1÷Z)
    "sign-quot-residue": [("src/casim.c", "            if (sd == 2 && !any_letter(c, ld.v)) return lm(L_UND, 0);  /* CAS 022: a residue isn't a divisor */\n", "")],
    # CAS 023 (work/023-conditions.md): each site's row not noted (site 3, the ∞ leading terms, isn't reached: the walk
    # refuses ∞ times a letter first, so its mutant is left out)
    "cond-site1-quot": [("src/casim.c", "                    if (sd == 2) lm_note(c, L, ld.v, '!');           /* CAS 023 site 1 */",
                         "                    (void)0;")],
    "cond-site2-lhopital": [("src/casim.c", "            if (c->st == CASIM_OK && L->lt) {                 /* CAS 023 site 2: d' over the line */",
                             "            if (0) {")],
    "cond-site4-sides": [("src/casim.c", "                    lm_note(&c, &L, add2(&c, up.v, ib_neg(&c, dn.v)), '!');   /* CAS 023 site 4: the sides apart */",
                          "                    (void)0;")],
    "cond-site5-continuity": [("src/casim.c", "    if (ok && L->lt) lm_at_cd(c, f, L);                 /* CAS 023 site 5 */",
                               "    if (0) lm_at_cd(c, f, L);")],
    "cond-site6-answer": [("src/casim.c", "        if (r.k == L_FIN) cd_walk(&c, canon(&c, r.v), &W);", "        (void)0;")],
    "cond-site7-f": [("src/casim.c", "        cd_walk(&c, f, &W);\n    }", "        (void)0;\n    }")],
    "cond-divisor": [("src/casim.c", "    if (kind == K_BDIV) u = nd(c, r)->b;", "    if (0) u = nd(c, r)->b;")],
    "cond-corner": [("src/casim.c", "    else if (f_ && (id == F_INV || id == F_TAN ||\n                    (W->corner && (id == F_ABS || id == F_SGN) && contains(c, canon(c, nd(c, r)->a), W->var))))\n",
                     "    else if (f_ && (id == F_INV || id == F_TAN))\n")],
    # abacus #9525 (h023's L053): a corner only where its argument holds var (ABS(K)×SQRT(1+X)), and never at order 0
    "cond-corner-constant": [("src/casim.c", " && contains(c, canon(c, nd(c, r)->a), W->var))))\n", ")))\n")],
    "cond-corner-order0": [("src/casim.c", "        cdwalk W = {&C, var, a, n > 0};", "        cdwalk W = {&C, var, a, true};")],
    "cond-tan-not-cos": [("src/casim.c", "    if (f_ && id == F_TAN) u = fn(c, F_COS, u);", "    (void)0;")],
    "cond-domain-as-ne": [("src/casim.c", "    else if (f_ && (id == F_LN || id == F_LOG || id == F_SQRT)) u = nd(c, r)->a, op = '>';",
                           "    else if (f_ && (id == F_LN || id == F_LOG || id == F_SQRT)) u = nd(c, r)->a;")],
    "cond-root-as-ne": [("src/casim.c", "            u = nd(c, r)->a, op = val(c, e)->q != 1 ? '>' : '!';", "            u = nd(c, r)->a, op = '!';")],
    # rule 3, the rows written
    "cond-product-unsplit": [("src/casim.c", "        for (int i = 0; i < nd(c, e)->n; i++) cd_ne(c, rows, n, list(c, nd(c, e)->a)[i]);",
                              "        sm_row(c, rows, n, e, '!', small(c, C_0), 26);")],
    "cond-unsolved": [("src/casim.c", "    if (!cd_mono(c, e, &k, &L, &m, &c0)) {\n        sm_row(c, rows, n, e, '!', small(c, C_0), 26);",
                       "    if (1) {\n        sm_row(c, rows, n, e, '!', small(c, C_0), 26);")],
    "cond-root-sign": [("src/casim.c", "    if (m % 2) sm_row(c, rows, n, leaf(c, K_VAR, L), '!', t.p < 0 ? ib_canon(c, ib_neg(c, u)) : u, L);",
                        "    if (m % 2) sm_row(c, rows, n, leaf(c, K_VAR, L), '!', t.p < 0 ? u : ib_canon(c, ib_neg(c, u)), L);")],
    "cond-square-gt": [("src/casim.c", "        if (k.p > 0 && t.p == 0) {", "        if (0) {")],
    # rule 4, the implication: the same expression's > not kept; a > row not leaving out its ≠ (abacus #9493 (4))
    "cond-gt-not-kept": [("src/casim.c", "            if (op == '>') S->op[i] = '>';", "            (void)0;")],
    "cond-gt-says-not-ne": [("src/casim.c", "        if (said) continue;", "        (void)said;")],
    # rule 2: rows only with a letter (abacus #9493 (4)), never on a refusal. Left out, run and surviving: lm_combine's give-back
    # of an abandoned attempt's notes (a letter limit reaches lm_combine only through an ∞-∞ of known signs, which letters
    # make unknown) and the series route's reset of the walk's notes (they point into the arena the route reuses, and
    # site 7 notes what the route divides by); 48 rows aimed at both, at two arenas, route on and off, 192 runs: 0 differ
    "cond-letter-free-walk": [("src/casim.c", "    if (c.st == CASIM_OK && !w.full && raw_letter(&c, raw, var)) {",
                               "    if (c.st == CASIM_OK && !w.full && raw) {")],
    "cond-letter-free-row": [("src/casim.c", "    if (c->st != CASIM_OK || contains(c, e, var) || !any_letter(c, e)) return;",
                              "    if (c->st != CASIM_OK || contains(c, e, var)) return;")],
    "cond-rows-on-refusal": [("src/casim.c", "    if (L.C.n && r.k != L_UND) {", "    if (L.C.n) {")],
    # rule 6, the sums: the refusal off, and applied to i^K and to a number base
    "cond-sums-off": [("src/casim.c", "            if (!is_num(c, base) && !any_letter(c, base) && !contains(c, base, 26)) c->st = CASIM_UNDECIDED;",
                       "            (void)0;")],
    "cond-sums-i": [("src/casim.c", "            if (!is_num(c, base) && !any_letter(c, base) && !contains(c, base, 26)) c->st = CASIM_UNDECIDED;",
                     "            if (!is_num(c, base) && !any_letter(c, base)) c->st = CASIM_UNDECIDED;")],
    "cond-sums-number": [("src/casim.c", "            if (!is_num(c, base) && !any_letter(c, base) && !contains(c, base, 26)) c->st = CASIM_UNDECIDED;",
                          "            if (!any_letter(c, base) && !contains(c, base, 26)) c->st = CASIM_UNDECIDED;")],
    # CAS 024 (work/024-ratio-shift.md; abacus #9560, #9572): a ratio in two letters or more not shifted; rule 2's cap
    # off (the largest s_L, past lo); the cap for one letter too; a letter at a negative power read as positive (X÷Y);
    # s below a row-free letter's s_L (the clamp, as #9572 restated it); lo < 0 not shifted; a shift to ∞ too. A mutant
    # that drops the last use of inf keeps (void)inf, or -Werror stops its build (the 414 on de84a69 lost two so)
    "ratio-two-letters-off": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                               "    (void)inf; if (nl > 1) s = 0;")],
    "ratio-cap-off": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                       "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : s;")],
    "ratio-cap-one-letter": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                              "    if (nl >= 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;")],
    "ratio-negpow-as-positive": [("src/casim.c", "val(c, x->b)->q == 1 && val(c, x->b)->p > 0 &&"
                                  " val(c, x->b)->p <= SM_DEG)\n"
                                  "            L = nd(c, x->a)->id, m = (int)val(c, x->b)->p;",
                                  "val(c, x->b)->q == 1 && val(c, x->b)->p != 0 && val(c, x->b)->p <= SM_DEG)\n"
                                  "            L = nd(c, x->a)->id, m = val(c, x->b)->p < 0 ? -(int)val(c, x->b)->p :"
                                  " (int)val(c, x->b)->p;")],
    "ratio-clamp-below": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                           "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap - (cap > 0);")],
    "ratio-neglo-off": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                         "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? 0 : cap;")],
    "ratio-neglo-at-inf": [("src/casim.c", "    if (nl > 1) s = !whole ? 0 : low < 0 && !inf ? low : cap;",
                            "    (void)inf; if (nl > 1) s = !whole ? 0 : low < 0 ? low : cap;")],
    # abacus #9594 (h024's R083): a letter a term from lo divides by keeps its row to ∞; the row from the clamped s_L
    # (a letter q holds at a positive power read as s_L 0: rows for nothing at a negative lo)
    "ratio-rowl-off": [("src/casim.c", "            if (!neg && G[gi].rowl >> l & 1u) {", "            if (0) {")],
    "ratio-rowl-clamped": [("src/casim.c",
                            "        if (whole && (lp < 0 ? (-lp + m - 1) / m : -(lp / m)) > low) rl |= 1u << L;",
                            "        if (whole && sl > low) rl |= 1u << L;")],
    # CAS 025 (work/025-every-ratio-rows.md; abacus #9687): the rows not read past another factor (rc6's early return);
    # the rows set after the return; the flag ignored, so B is shifted past that factor; a letter inside a function read
    # as a letter at a whole power; a letter at a negative power without its row; and that row for a monomial too,
    # whose own row ABS(X)>v already excludes X = 0. (Its row for a symbolic lo too isn't a mutant: the answer then
    # holds (X÷Y)^A, so the probe prints Y≠0 anyway)
    "ratio025-early-return": [("src/casim.c", "            other = true;\n            continue;\n        }\n",
                               "            return 0;\n        }\n")],
    "ratio025-rowl-after-return": [("src/casim.c", "    g->rowl = rl;\n    if (other) return 0;\n",
                                    "    if (other) return 0;\n    g->rowl = rl;\n")],
    "ratio025-other-ignored": [("src/casim.c", "    if (other) return 0;\n", "    (void)other;\n")],
    "ratio025-fn-letter-whole": [("src/casim.c", "        if (x->kind == K_VAR && x->id < 26) L = x->id, m = 1;\n",
                                  "        if (x->kind == K_VAR && x->id < 26) L = x->id, m = 1;\n"
                                  "        else if (x->kind == K_FN && nd(c, x->a)->kind == K_VAR && nd(c, x->a)->id < 26)"
                                  " L = nd(c, x->a)->id, m = 1;\n")],
    "ratio025-negpow-row-off": [("src/casim.c", "                rl |= 1u << nd(c, x->a)->id;\n", "                (void)0;\n")],
    "ratio025-negpow-mono-row": [("src/casim.c", "            if (!neg && G[gi].rowl >> l & 1u) {",
                                  "            if ((void)neg, G[gi].rowl >> l & 1u) {")],
    # CAS 026 (work/026-factor-rows.md; decision 90, abacus #9720/#9726): the factor row not made; rule 2's skip off (a row
    # where the answer has no value already: the 8 to N go TOO_LONG); rule 3's off (EXP(Z)≠0); a fraction power read
    # whole only (SQRT(Z)^(3×K-1)'s Z^(3÷2)); a negative fraction power read as a positive one ((Y÷SQRT(Z))^K); a root
    # probed by replacement, its bare Z left free; the merged root's letter path off; the factor rows not asked at all
    "factor026-row-off": [("src/casim.c", "        sm_row(c, rows, n, h, '!', small(c, C_0), x->kind == K_VAR ? x->id : 26);",
                           "        (void)rows, (void)n;")],
    "factor026-hole-kept": [("src/casim.c", "            compact(c, mark, sum);\n            continue;\n",
                             "            compact(c, mark, sum);\n")],
    "factor026-never-zero-off": [("src/casim.c",
                                  "        if (x->kind == K_FN && (1u << x->id) & (1u << F_EXP | 1u << F_ALOG | 1u << F_COSH)) continue;",
                                  "")],
    "factor026-floor-whole": [("src/casim.c", "            return t >= 0 ? t / u : -((-t + u - 1) / u);",
                               "            return t % u ? 0 : t / u;")],
    "factor026-negfrac-positive": [("src/casim.c",
                                    "is_num(c, nd(c, f)->b) && val(c, nd(c, f)->b)->p < 0) {",
                                    "is_num(c, nd(c, f)->b) && val(c, nd(c, f)->b)->p < 0 && !mf) {")],
    "factor026-root-replaced": [("src/casim.c", "        else if (nd(c, x->a)->kind == K_VAR && ((x->kind == K_POW",
                                 "        else if (0 && nd(c, x->a)->kind == K_VAR && ((x->kind == K_POW")],
    "factor026-letter-off": [("src/casim.c", "        if (x->kind == K_VAR && (mf < 0 || S->a < 0)) continue;",
                              "        if (x->kind == K_VAR) continue;")],
    "factor026-not-asked": [("src/casim.c",
                             "    for (int gi = 0; gi < ng && c->st == CASIM_OK; gi++) sm_rowf(c, S, &G[gi], sum, rows, n);   /* CAS 026 */",
                             "    (void)sm_rowf;              /* referenced, or -Werror refuses the build and nothing is tested */")],
    # CAS 027 (work/027-function-factor-shift.md; abacus #10004, #10068): the factor not read (sm_atom refusing every
    # factor that holds a letter); rule 2 off (a shift past lo); rule 4 off (CAS 026's row test at the unshifted lo);
    # the shift read from the wrong bits; a never-0 factor read as one with zeros (EXP(Z) shifted past, its X≠0
    # dropped); sm_powf's floor read as a ceiling; the atom's s rounded down, so the shift doesn't clear the factor
    "factor027-atom-off": [("src/casim.c", "    if (m <= 0 || m > SM_DEG || !any_letter(c, h)) return -1;",
                            "    if (m <= 0 || m > SM_DEG || any_letter(c, h)) return -1;")],
    "factor027-rule2-off": [("src/casim.c", "    if (rl >> 31 && (!whole || s > low)) return 0;",
                             "    if (rl >> 31 && !whole) return 0;")],
    "factor027-rule4-off": [("src/casim.c", "e + mf * ((int)S->a - (((int)(g->rowl >> 26 & 31u) ^ 16) - 16)) >= 0",
                             "e + mf * (int)S->a >= 0")],
    "factor027-shift-bits": [("src/casim.c", "(((int)(g->rowl >> 26 & 31u) ^ 16) - 16)",
                              "(((int)(g->rowl >> 27 & 31u) ^ 16) - 16)")],
    "factor027-never-zero-read": [("src/casim.c",
                                   "    if (x->kind == K_FN && (1u << x->id) & (1u << F_EXP | 1u << F_ALOG | 1u << F_COSH)) "
                                   "return -1;\n", "")],
    "factor027-powf-ceiling": [("src/casim.c", "            return t >= 0 ? t / u : -((-t + u - 1) / u);",
                                "            return t >= 0 ? (t + u - 1) / u : -(-t / u);")],
    "factor027-atom-s-floor": [("src/casim.c",
                                "        int e = sm_powf(c, g->q[j], h);\n        if (e < 0 && (-e + m - 1) / m > sl) sl = (-e + m - 1) / m;",
                                "        int e = sm_powf(c, g->q[j], h);\n        if (e < 0 && -e / m > sl) sl = -e / m;")],
    # CAS 028 (solve's roots left out, ", n CPLX"): the note off; at the cap, TOO_LONG in place of today's line, the note
    # kept over the roots' last bytes (it ignores the cap, abacus #10167), and the note giving way one byte early; the
    # multiplicity ignored; NO REAL ROOTS noted; PARTIAL with no root unnoted; every quadratic counted, so the bit's
    # pairs and the real ones too; the note after the roots, not the status
    "cplx028-note-off": [("src/casim.c", "    if (out && (nr || f->n >= 3)) put_left_out(w, end, out);",
                          "    if (out && (nr || f->n >= 3) && 0) put_left_out(w, end, out);")],
    "cplx028-cap-too-long": [("src/casim.c", "    if (w->full || w->n + k + 1 > w->cap) return;",
                              "    if (w->full) return;\n    if (w->n + k + 1 > w->cap) {\n        w->full = true;\n        return;\n    }")],
    "cplx028-cap-ignored": [("src/casim.c", "    if (w->full || w->n + k + 1 > w->cap) return;",
                             "    if (w->full) return;\n    if (w->n + k + 1 > w->cap) w->n = w->cap - 1 - k, w->o[w->n] = '\\0';")],
    "cplx028-cap-early": [("src/casim.c", "    if (w->full || w->n + k + 1 > w->cap) return;",
                           "    if (w->full || w->n + k + 2 > w->cap) return;")],
    "cplx028-mult-ignored": [("src/casim.c", "            else if (nr == k) out += 2 * x->mult;",
                              "            else if (nr == k) out += 2;")],
    "cplx028-no-real-roots-noted": [("src/casim.c", "    if (out && (nr || f->n >= 3)) put_left_out(w, end, out);",
                                     "    if (out) put_left_out(w, end, out);")],
    "cplx028-partial-unnoted": [("src/casim.c", "    if (out && (nr || f->n >= 3)) put_left_out(w, end, out);",
                                 "    if (out && nr) put_left_out(w, end, out);")],
    "cplx028-every-quadratic": [("src/casim.c", "            else if (nr == k) out += 2 * x->mult;",
                                 "            else out += 2 * x->mult, (void)k;")],
    "cplx028-after-the-roots": [("src/casim.c", "    if (out && (nr || f->n >= 3)) put_left_out(w, end, out);",
                                 "    if (out && (nr || f->n >= 3)) put_left_out(w, w->n, out), (void)end;")],
}


FRAME_CAUGHT = {"irat-hook-in-do-integrate"}       # its fault is a frame (the hook's locals in do_integrate's)


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")


def main():
    anchors = sys.argv[1:] == ["--anchors"]
    if anchors and os.path.basename(os.path.dirname(ROOT)) == "mutants":
        return 0                    # a mutant's own copy: its anchor is gone by design
    names = [] if anchors else sys.argv[1:]
    names = names or list(MUTANTS)
    # every anchor once in its own file before an hour of builds: a refactor can copy or move a mutant's text
    stale = [f"{name}: its text appears {open(os.path.join(ROOT, path), encoding='utf-8').read().count(old)} times "
             f"in {path}" for name in names for path, old, new in MUTANTS[name]
             if open(os.path.join(ROOT, path), encoding="utf-8").read().count(old) != 1]
    if stale:
        print(("FAIL scripts/mutants.py: stale anchors" if anchors else "NOT RUN, stale anchors") + ":\n  " +
              "\n  ".join(stale))
        return 1
    if anchors:
        print(f"mutant anchors: {len(names)} mutants, each anchor found once")
        return 0
    # a copy can't fetch the core (CORE_REPO is relative), so make test there runs the developer tests only on a core
    # built here first; without one each copy's make test stops before them (the 414 on v1.0.0-rc6: six "survived")
    pin = os.path.join(ROOT, "build", "core", ".pin")
    want = open(os.path.join(ROOT, "CORE_PIN"), encoding="utf-8").read().strip()
    if not os.path.exists(pin) or open(pin, encoding="utf-8").read().strip() != want:
        print(f"NOT RUN, no core at {want[:7]} in build/: run make test here first")
        return 1
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
        if name not in FRAME_CAUGHT:
            run([sys.executable, "scripts/frame-table.py", "--record"], tree)
        test = run(["make", "-s", "test"], tree)
        caught = [ln.split()[1] for ln in test.stdout.splitlines() if ln.startswith(("FAIL ", "REFUSED "))]
        if test.returncode != 0 and caught:
            print(f"caught   {name}: by {', '.join(c.rstrip(':') for c in caught)}")
            shutil.rmtree(tree, ignore_errors=True)
        elif test.returncode != 0:
            print(f"NOT TESTED {name} (make test stopped with no FAIL line, not counted):\n{test.stdout[-800:]}")
            survived += 1
        else:
            print(f"SURVIVED {name}")
            survived += 1
    print(f"{len(names) - survived} of {len(names)} mutants caught")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())

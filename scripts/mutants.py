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
        ("src/casim.c", "    return do_solve(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);",
         "    return do_solve(eqn, var, angle, out, cap, arena, arena_cap, ((void)model, false));"),
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
        ("src/casim.c", "    if (rat_is(n, -1, 1)) return mul2(c, fn(c, F_LN, fn(c, F_ABS, uc)), power(c, num(c, &lin[1]), small(c, C_M1)));",
         "    if (rat_is(n, -1, 1)) return mul2(c, fn(c, F_LN, uc), power(c, num(c, &lin[1]), small(c, C_M1)));"),
    ],
    "integ-deg-factor-lost": [
        ("src/casim.c", "        case F_COS: r = mul2(c, fn(c, F_SIN, uc), angle_factor(c, true)); break;",
         "        case F_COS: r = fn(c, F_SIN, uc); break;"),
    ],
    "integ-verification-skipped": [
        ("src/casim.c", "    return c->st == CASIM_OK && is_k(c, t, 0, 1);\n}", "    return c->st == CASIM_OK || is_k(c, t, 0, 1);\n}"),
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
        ("src/casim.c", "                    list(c, l)[i] = power(c, g->a, small(c, C_M1));\n                    list(c, l)[j] = small(c, C_1);",
         "                    (void)l;"),
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
        ("src/casim.c", "        else if (!rat_is(&content, 1, 1)) put_content(&o, &content);", "        (void)put_content;"),
    ],
    "factor-content-sign-flipped": [
        ("src/casim.c", "        if (rat_is(&content, -1, 1)) puts_(&o, \"-1\");", "        if (content.p < 0) content.p = -content.p;\n        if (rat_is(&content, -1, 1)) puts_(&o, \"-1\");"),
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
        ("src/casim.c", "        c->st = raw_has_var(c, r, var) ? CASIM_NOT_POLYNOMIAL : CASIM_NOT_NUMERIC;",
         "        c->st = (raw_has_var(c, r, var), CASIM_NOT_NUMERIC);"),
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
        ("src/casim.c", "            } else if (!rat_is(&g, 1, 1))\n                r = mul2(c, num(c, &g), r);", "            }"),
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
        ("src/casim.c", "    return st == CASIM_NOT_INTEGRABLE ? do_irational(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU) : st;",
         "    return st;"),
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
    "limit-lhopital-not-indeterminate": [("src/casim.c", "                if (ln.k == L_FIN) return lm(L_FIN, canon(c, mul2(c, ln.v, power(c, ld.v, small(c, C_M1)))));\n", "")],
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
}


FRAME_CAUGHT = {"irat-hook-in-do-integrate"}       # its fault is a frame (the hook's locals in do_integrate's)


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
        if name not in FRAME_CAUGHT:
            run([sys.executable, "scripts/frame-table.py", "--record"], tree)
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

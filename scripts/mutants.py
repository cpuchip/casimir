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
        ("src/casim.c", "    return do_integrate(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);",
         "    return do_integrate(eqn, var, angle, out, cap, arena, arena_cap, ((void)model, false));"),
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

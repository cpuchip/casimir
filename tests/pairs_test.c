/* Our own pairs, shaped like abacus's CAS 002 oracle: a random expression printed twice, the second
 * time with every sum and product commuted and regrouped and some a-b written as -b+a, at every depth.
 * The two texts are the same mathematics, so their simplified texts must be identical; the result must
 * keep the core's value of the first (1E-24, the unit's tolerance) and be a fixed point. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define CASES 3000
#define OUT 8192
static unsigned char arena[65536];
static unsigned long long rng = 0x2545F4914F6CDD1Dull;

static unsigned rnd(unsigned n)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (unsigned)(rng % n);
}

enum { G_LEAF, G_FN, G_SUM, G_PROD, G_SUB, G_DIV, G_POW, G_NEG, G_TWICE };

typedef struct g {
    int kind, n;
    const char *s;          /* a leaf's text, a function's name, a power's exponent */
    struct g *k[3];
} g;

static g pool[512];
static int used;

static const char *const LEAF[] = {"X", "Y", "X", "Y", "2", "3", "0.5", "1.5", "0.25", "5", "7", "π"};
static const char *const FN[] = {"SQ", "SQRT", "INV", "LN", "LOG", "EXP", "ALOG", "SIN", "COS", "TAN", "ATAN",
                                 "SINH", "COSH", "TANH", "ASINH", "ABS"};
static const char *const EXP_[] = {"2", "3", "0.5", "-1", "1.5", "-0.5"};

static g *gen(int depth)
{
    g *t = &pool[used++];
    memset(t, 0, sizeof *t);
    t->kind = depth <= 0 || used > 400 ? G_LEAF : (int)rnd(9);
    switch (t->kind) {
    case G_LEAF: t->s = LEAF[rnd(sizeof LEAF / sizeof LEAF[0])]; break;
    case G_FN: t->s = FN[rnd(sizeof FN / sizeof FN[0])]; t->n = 1; break;
    case G_SUM:
    case G_PROD: t->n = 2 + (int)rnd(2); break;
    case G_SUB:
    case G_DIV: t->n = 2; break;
    case G_POW: t->s = EXP_[rnd(6)]; t->n = 1; break;
    case G_NEG: t->n = 1; break;
    case G_TWICE: t->s = EXP_[rnd(6)]; t->n = 1; break;
    }
    for (int i = 0; i < t->n; i++) t->k[i] = gen(depth - 1 - (int)rnd(2));
    return t;
}

static void put(char *o, size_t cap, const char *s)
{
    size_t n = strlen(o), m = strlen(s);
    if (n + m < cap) memcpy(o + n, s, m + 1);
}

static void print(g *t, char *o, size_t cap, bool shuffle);

static void paren(g *t, char *o, size_t cap, bool shuffle)
{
    put(o, cap, "(");
    print(t, o, cap, shuffle);
    put(o, cap, ")");
}

/* A, as generated; or B, with operands commuted and regrouped and a-b sometimes as -b+a. */
static void print(g *t, char *o, size_t cap, bool shuffle)
{
    switch (t->kind) {
    case G_LEAF: put(o, cap, t->s); return;
    case G_FN: put(o, cap, t->s); paren(t->k[0], o, cap, shuffle); return;
    case G_NEG: put(o, cap, "-"); paren(t->k[0], o, cap, shuffle); return;
    case G_POW:
        paren(t->k[0], o, cap, shuffle);
        put(o, cap, "^");
        put(o, cap, t->s);
        return;
    case G_DIV: paren(t->k[0], o, cap, shuffle); put(o, cap, "÷"); paren(t->k[1], o, cap, shuffle); return;
    case G_TWICE:
        /* one subtree twice, under powers: like factors whose merge makes another base (the review's
           idempotence finding: no generator built a repeated base before) */
        put(o, cap, "((");
        print(t->k[0], o, cap, shuffle);
        put(o, cap, ")^");
        put(o, cap, t->s);
        put(o, cap, ")×((");
        print(t->k[0], o, cap, shuffle);
        put(o, cap, ")^");
        put(o, cap, t->s);
        put(o, cap, ")");
        return;
    case G_SUB:
        if (shuffle && rnd(2)) {
            put(o, cap, "(-");
            paren(t->k[1], o, cap, shuffle);
            put(o, cap, ")+");
            paren(t->k[0], o, cap, shuffle);
        } else {
            paren(t->k[0], o, cap, shuffle);
            put(o, cap, "-");
            paren(t->k[1], o, cap, shuffle);
        }
        return;
    default: {
        const char *op = t->kind == G_SUM ? "+" : "×";
        int ord[3] = {0, 1, 2};
        if (shuffle)
            for (int i = t->n - 1; i > 0; i--) {
                int j = (int)rnd((unsigned)i + 1), x = ord[i];
                ord[i] = ord[j], ord[j] = x;
            }
        if (t->n == 3 && shuffle && rnd(2)) {
            /* a(bc) rather than (ab)c */
            paren(t->k[ord[0]], o, cap, shuffle);
            put(o, cap, op);
            put(o, cap, "(");
            paren(t->k[ord[1]], o, cap, shuffle);
            put(o, cap, op);
            paren(t->k[ord[2]], o, cap, shuffle);
            put(o, cap, ")");
            return;
        }
        for (int i = 0; i < t->n; i++) {
            if (i) put(o, cap, op);
            paren(t->k[ord[i]], o, cap, shuffle);
        }
        return;
    }
    }
}

/* Whether text is exactly 0 at the other test point (X and Y each swapped), the point restored after. */
static bool zero_elsewhere(const char *text, const char *xs, const char *ys)
{
    abn v;
    bool zero;
    ce_set('X', xs[0] == '0' ? "1.4" : "0.71");
    ce_set('Y', ys[1] == '.' && ys[0] == '0' ? "1.2" : "0.45");
    zero = ce_eval(text, &v) == ABN_OK && abn_is_zero(&v);
    ce_set('X', xs);
    ce_set('Y', ys);
    return zero;
}

int main(void)
{
    int judged = 0, conditioned = 0;
    ce_init();
    for (int c = 0; c < CASES; c++) {
        char a[OUT] = "", b[OUT] = "", sa[OUT], sb[OUT], again[OUT];
        const char *xs, *ys;
        int angle = (int)rnd(3);
        abn fa, fb, v;
        casim_status ra, rb;
        g *t;
        used = 0;
        t = gen(1 + (int)rnd(5));
        print(t, a, sizeof a, false);
        print(t, b, sizeof b, true);
        if (strlen(a) > 3000 || strlen(b) > 3000) continue;
        ce_angle(angle);
        xs = rnd(2) ? "0.71" : "1.4";
        ys = rnd(2) ? "0.45" : "1.2";
        ce_set('X', xs);
        ce_set('Y', ys);
        /* the case, like abacus's: both texts are defined and real here */
        if (ce_eval(a, &fa) != ABN_OK || ce_eval(b, &fb) != ABN_OK) continue;
        if (!ce_close(&fb, &fa, -24)) continue;     /* the two texts' own rounding differs too much */
        judged++;
        ra = casim_simplify(a, angle, sa, sizeof sa, arena, sizeof arena);
        rb = casim_simplify(b, angle, sb, sizeof sb, arena, sizeof arena);
        CHECK(ra == CASIM_OK && rb == CASIM_OK, "status %d / %d:\n  %s\n  %s", (int)ra, (int)rb, a, b);
        if (ra != CASIM_OK || rb != CASIM_OK) continue;
        CHECK(strcmp(sa, sb) == 0, "pair differs (angle %d):\n  A %s\n  -> %s\n  B %s\n  -> %s", angle, a, sa, b, sb);
        /* the value: within 1E-24 (the unit's tolerance), or else counted as conditioning when within 1E-12:
           the core rounds the input's arithmetic one way and the simplified text's another, and a function
           of a large argument (SIN of 4.7E15 from nested powers of 1.5) magnifies that past 1E-24 while
           both are right. Beyond 1E-12 a simplification is wrong; more than 1% in the band fails too. */
        if (ce_eval(sa, &v) == ABN_OK && !ce_close(&v, &fa, -24) &&
            (ce_close(&v, &fa, -12) || (abn_is_zero(&v) && zero_elsewhere(a, xs, ys)))) {
            /* (or an exact 0 against a rounding residue, which has no scale to be close in: the core gives
               X-X^0.5×X^0.5 as 1E-33 at X = 1.4 and 0 at 0.71, since core 2f2eff6; the input is exactly 0 at
               the other point) */
            conditioned++;
            printf("conditioning (angle %d): %s = %s is %s, the input %s\n", angle, a, sa, ce_text(&v), ce_text(&fa));
        } else
            CHECK(ce_eval(sa, &v) == ABN_OK && ce_close(&v, &fa, -24), "value (angle %d): %s = %s is %s, want %s", angle,
                  a, sa, ce_text(&v), ce_text(&fa));
        CHECK(casim_simplify(sa, angle, again, sizeof again, arena, sizeof arena) == CASIM_OK && strcmp(again, sa) == 0,
              "not idempotent: %s -> %s", sa, again);
    }
    printf("pairs: %d of %d judged, %d in the conditioning band\n", judged, CASES, conditioned);
    CHECK(conditioned * 100 <= judged, "%d of %d in the conditioning band (over 1%%)", conditioned, judged);
    CHECK(judged >= CASES / 3, "only %d of %d pairs judged", judged, CASES);
    return check_done();
}

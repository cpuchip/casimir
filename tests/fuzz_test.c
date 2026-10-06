/* A second instrument, shaped differently from the rule tests: random expressions in the core's
 * grammar (our own generator), each derivative compared with a numeric one taken through the core
 * itself. The numeric derivative is the central difference at h and h/2, extrapolated (Richardson:
 * (4·D(h/2) − D(h))/3, error O(h^4)); a case counts only where the two differences agree (the
 * function is smooth there). Every expression also round-trips through casim's parser and printer
 * to the same value, digit for digit. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define CASES 3000
static unsigned char arena[32768];
static unsigned long long rng = 0x9E3779B97F4A7C15ull;

static unsigned rnd(unsigned n)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (unsigned)(rng % n);
}

static const char *const FN[] = {"SQ", "SQRT", "INV", "LN", "LOG", "EXP", "ALOG", "SIN", "COS", "TAN", "ASIN",
                                 "ACOS", "ATAN", "SINH", "COSH", "TANH", "ASINH", "ACOSH", "ATANH", "ABS"};
static const char *const NUM[] = {"2", "3", "0.5", "1.5", "0.25", "5", "1", "0", "1E-1", ".75", "1.1.4"};
static const char *const OP[] = {"+", "-", "×", "÷", "^"};

/* o += s, within cap (never reached at these depths). */
static void put(char *o, size_t cap, const char *s)
{
    size_t n = strlen(o), m = strlen(s);
    if (n + m < cap) memcpy(o + n, s, m + 1);
}

/* An expression of the given depth; parenthesized or not at random, so precedence is exercised. */
static void gen(char *o, size_t cap, int depth)
{
    unsigned k = depth <= 0 ? rnd(3) : rnd(10);
    char a[1024] = "", b[1024] = "";
    o[0] = '\0';
    if (k == 0) put(o, cap, rnd(2) ? "X" : "Y");
    else if (k == 1) put(o, cap, NUM[rnd(sizeof NUM / sizeof NUM[0])]);
    else if (k == 2) put(o, cap, rnd(4) ? "X" : "π");
    else if (k <= 5) {
        gen(a, sizeof a, depth - 1);
        put(o, cap, FN[rnd(sizeof FN / sizeof FN[0])]), put(o, cap, "("), put(o, cap, a), put(o, cap, ")");
    } else if (k == 6) {
        bool paren = rnd(2);
        gen(a, sizeof a, depth - 1);
        put(o, cap, paren ? "-(" : "-"), put(o, cap, a), put(o, cap, paren ? ")" : "");
    } else {
        const char *op = OP[rnd(5)];
        gen(a, sizeof a, depth - 1);
        gen(b, sizeof b, depth - 1);
        if (*op == '^') {
            put(o, cap, "("), put(o, cap, a), put(o, cap, ")^");
            put(o, cap, rnd(3) ? "2" : rnd(2) ? "(X)" : "-0.5");
        } else if (rnd(2)) {
            put(o, cap, "("), put(o, cap, a), put(o, cap, ")"), put(o, cap, op);
            put(o, cap, "("), put(o, cap, b), put(o, cap, ")");
        } else put(o, cap, a), put(o, cap, op), put(o, cap, b);
    }
}

/* The core's value of text at var = x (other variables as set); false on any error. */
static bool at(const char *text, char var, const abn *x, abn *out)
{
    ce_set_abn(var, *x);
    return ce_eval(text, out) == ABN_OK;
}

/* Central difference (f(x+h) - f(x-h)) / 2h. */
static bool central(const char *text, char var, const abn *x, const abn *h, abn *out)
{
    abn xp, xm, fp, fm, d, two_h, two = abn_from_int(2);
    return abn_add(&xp, x, h) == ABN_OK && abn_sub(&xm, x, h) == ABN_OK && at(text, var, &xp, &fp) &&
           at(text, var, &xm, &fm) && abn_sub(&d, &fp, &fm) == ABN_OK && abn_mul(&two_h, &two, h) == ABN_OK &&
           abn_div(out, &d, &two_h) == ABN_OK;
}

int main(void)
{
    int used = 0, roundtrips = 0, refused_in_scope = 0;
    ce_init();
    for (int i = 0; i < CASES; i++) {
        char e[1024], out[4096], re[4096];
        char var = rnd(4) ? 'X' : 'Y';
        int angle = (int)rnd(3);
        abn x, h, h2, d1, d2, num, three = abn_from_int(3), four = abn_from_int(4), t, got, f0, r0;
        casim_status st;
        gen(e, sizeof e, 1 + (int)rnd(4));
        ce_angle(angle);
        ce_set('X', rnd(2) ? "0.6" : "0.35");
        ce_set('Y', rnd(2) ? "0.45" : "1.3");
        x = ce_calc->var[var - 'A'].re;
        if (ce_eval(e, &f0) != ABN_OK) continue;           /* outside the domain here */
        /* round trip */
        if (casim_reprint(e, re, sizeof re, arena, sizeof arena) == CASIM_OK) {
            roundtrips++;
            CHECK(ce_eval(re, &r0) == ABN_OK && abn_cmp(&r0, &f0) == 0, "reprint %s -> %s changed the value", e, re);
        } else CHECK(false, "reprint %s failed", e);
        st = casim_derive(e, var - 'A', angle, out, sizeof out, arena, sizeof arena);
        if (st != CASIM_OK) {
            refused_in_scope++;
            CHECK(false, "d/d%c %s (angle %d): status %d (%s)", var, e, angle, (int)st, out);
            continue;
        }
        /* the numeric derivative, where it is well defined */
        h = ce_num("1E-7"), h2 = ce_num("5E-8");
        if (!central(e, var, &x, &h, &d1) || !central(e, var, &x, &h2, &d2)) continue;
        ce_set_abn(var, x);
        if (!ce_close(&d1, &d2, -8)) continue;             /* not smooth at this point */
        if (abn_mul(&t, &four, &d2) != ABN_OK || abn_sub(&t, &t, &d1) != ABN_OK || abn_div(&num, &t, &three) != ABN_OK)
            continue;
        if (ce_eval(out, &got) != ABN_OK) {
            CHECK(false, "d/d%c %s = %s does not evaluate where the function does (angle %d)", var, e, out, angle);
            continue;
        }
        used++;
        CHECK(ce_close(&got, &num, -14), "d/d%c %s (angle %d) = %s is %s, numerically %s", var, e, angle, out,
              ce_text(&got), ce_text(&num));
    }
    printf("fuzz: %d derivatives judged numerically, %d round trips, %d refused\n", used, roundtrips, refused_in_scope);
    CHECK(used >= CASES / 3, "only %d of %d cases judged: the generator or the domain filter is too strict", used, CASES);
    return check_done();
}

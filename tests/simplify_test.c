/* Unit CAS 002, casim's own tests of simplify (docs/canonical-form.md is the contract): each rule alone
 * as exact text, exact folding, sorting inside function arguments, the domain rule, L=R, the 12 visible
 * pairs, and for every case: the result's value through the core's eqn_eval equals the input's, and
 * simplifying the result gives the result again. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[32768];

/* The core's values of a and b at the test points agree to 10^tol, or both are errors. */
static bool same_value(const char *a, const char *b, int angle, int tol)
{
    static const char *const XS[3] = {"0.3", "0.55", "1.7"};
    ce_angle(angle);
    for (int i = 0; i < 3; i++) {
        abn va, vb;
        abn_status sa, sb;
        ce_set('X', XS[i]);
        ce_set('Y', "1.3");
        ce_set('Z', "0.45");
        sa = ce_eval(a, &va);
        sb = ce_eval(b, &vb);
        if (sa != ABN_OK) continue;         /* the input is undefined here: the rule may extend */
        if (sb != ABN_OK || !ce_close(&vb, &va, tol)) return false;
    }
    return true;
}

static casim_status simp(const char *in, int angle, char *out)
{
    return casim_simplify(in, angle, out, OUT, arena, sizeof arena);
}

/* in simplifies to exactly want; want has in's value; want is a fixed point. */
static void is(const char *in, const char *want)
{
    char out[OUT], again[OUT];
    casim_status st = simp(in, CASIM_RAD, out);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "simplify %s: status %d \"%s\", want \"%s\"", in, (int)st, out,
          want);
    if (st != CASIM_OK) return;
    CHECK(same_value(in, out, CASIM_RAD, -26), "simplify %s = %s changes the value", in, out);
    CHECK(simp(out, CASIM_RAD, again) == CASIM_OK && strcmp(again, out) == 0, "not idempotent: %s -> %s -> %s", in,
          out, again);
}

/* a and b simplify to the same text, with a's value. */
static void pair(const char *a, const char *b, int angle)
{
    char oa[OUT], ob[OUT], again[OUT];
    casim_status sa = simp(a, angle, oa), sb = simp(b, angle, ob);
    CHECK(sa == CASIM_OK && sb == CASIM_OK && strcmp(oa, ob) == 0, "pair differs:\n  %s\n  -> %s\n  %s\n  -> %s", a, oa,
          b, ob);
    if (sa != CASIM_OK) return;
    CHECK(same_value(a, oa, angle, -24), "simplify %s = %s changes the value", a, oa);
    CHECK(simp(oa, angle, again) == CASIM_OK && strcmp(again, oa) == 0, "not idempotent: %s -> %s", oa, again);
}

static void rules(void)
{
    /* sums and products sorted: variables by letter, constants last in a sum, coefficient first */
    is("7+X", "X+7");
    is("Y+X", "X+Y");
    is("Y×X", "X×Y");
    is("π×X", "X×π");
    is("X×2", "2×X");
    is("Y×X+X", "X+X×Y");
    /* flattened */
    is("(X+(Y+Z))+1", "X+Y+Z+1");
    is("(X×(Y×Z))×2", "2×X×Y×Z");
    /* like terms */
    is("X+X", "2×X");
    is("2×X+3×X", "5×X");
    is("X×Y+Y×X", "2×X×Y");
    is("X-X", "0");
    is("2×X-X×2+Y", "Y");
    /* like factors */
    is("X×X", "X^2");
    is("X^2×X^3", "X^5");
    is("X×X^-1", "1");
    is("X÷X", "1");
    is("X^Y×X", "X^(Y+1)");
    is("2^X×2^Y", "2^(X+Y)");
    is("SQRT(X)×SQRT(X)", "X");
    /* a merge that makes a number or a product is flattened and folded again (the unit's idempotence
       mutant survived every generator until these: none of them made such a merge) */
    is("2^X×2^(1-X)×Y", "2×Y");
    is("SQRT(X×Y)×SQRT(X×Y)×Z", "X×Y×Z");
    /* subtraction and division as + and ×, printed back */
    is("X+-1×Y", "X-Y");
    is("-(X+Y)", "-X-Y");
    is("X-(Y-Z)", "X-Y+Z");
    is("X×Y^-1", "X÷Y");
    is("X^-2", "1÷X^2");
    is("X÷(Y+1)", "X÷(Y+1)");
    is("X×2÷3", "2×X÷3");
    is("X÷2", "0.5×X");
    is("X÷3", "X÷3");
    is("-X÷3", "-X÷3");
    /* a sum that is a factor is primitive: its first coefficient 1, the content in the coefficient */
    is("2×(X+1)×Y", "2×Y×(X+1)");
    is("(2×X+2)×Y", "2×Y×(X+1)");
    is("(-X-1)×Y", "-Y×(X+1)");
    is("(2×X+2)^2", "4×(X+1)^2");
    is("(2×X+2)×(X+1)", "2×(X+1)^2");
    is("2×(X+1)", "2×X+2");                 /* alone, a number times a sum is distributed */
    is("-(-X-π+0.5)÷X", "(X+π-0.5)÷X");
    /* SQ, INV, SQRT */
    is("SQ(X)", "X^2");
    is("INV(X)", "1÷X");
    is("SQRT(X)", "SQRT(X)");
    is("INV(SQRT(X))", "1÷SQRT(X)");
    /* powers */
    is("(X×Y)^2", "X^2×Y^2");
    is("(X^2)^3", "X^6");
    is("(X^0.5)^2", "X");
    is("(2×X)^1.5", "(2×X)^1.5");      /* a product under a power that is not whole keeps its () */
    is("X^1", "X");
    is("X^0", "1");
    /* functions */
    is("LN(EXP(X))", "X");
    is("EXP(LN(X))", "X");
    is("LOG(ALOG(X))", "X");
    is("ALOG(LOG(X))", "X");
    is("SIN(Y+X)", "SIN(X+Y)");
    is("ABS(-3)", "3");
    /* L=R, each side */
    is("X+X=Y+X", "2×X=X+Y");
}

/* Exact folding: rationals, never binary floating point (0.1+0.2 is 0.3, not 0.30000000000000004). */
static void folding(void)
{
    is("0.1+0.2", "0.3");
    is("1÷3+1÷3", "2÷3");
    is("1÷3×3", "1");
    is("0.1×3", "0.3");
    is("2^3", "8");
    is("2^-2", "0.25");
    is("(1÷3)^2", "1÷9");
    is("(-2)^3", "-8");
    is("1.5E-3+1", "1.0015");
    is("0.1+0.2-0.3", "0");
    is("1÷7+X+6÷7", "X+1");
    is("2÷3×X+X÷3", "X");
    /* a product too long for 34 digits is not folded, and stays exact */
    {
        char out[OUT];
        CHECK(simp("123456789012345678901234567890×98765432109876543210987654321", CASIM_RAD, out) == CASIM_OK &&
                  same_value("123456789012345678901234567890×98765432109876543210987654321", out, CASIM_RAD, -30),
              "a 59-digit product: %s", out);
    }
}

/* Sorting at every depth, function arguments included (one of the unit's mutants). */
static void nesting(void)
{
    pair("SIN(COS(Y+X)×Z)", "SIN(Z×COS(X+Y))", CASIM_RAD);
    pair("EXP(LN(SQRT(Y+X+Z)))+TAN(X×Y×Z)", "TAN(Z×(Y×X))+EXP(LN(SQRT(Z+(X+Y))))", CASIM_RAD);
    pair("COS(COS(COS(COS(Y+X))))", "COS(COS(COS(COS(X+Y))))", CASIM_RAD);
    pair("SQ(ABS(X-Y))", "SQ(ABS(-Y+X))", CASIM_RAD);
    pair("ATAN(X×(Y+Z))", "ATAN((Z+Y)×X)", CASIM_DEG);
}

/* The domain rule: extend where the input is undefined, never change a value where it is defined. */
static void domain(void)
{
    char out[OUT];
    is("SQRT(SQ(X))", "SQRT(X^2)");     /* |X|, not X */
    is("(X^2)^0.5", "SQRT(X^2)");
    CHECK(simp("SQRT(SQ(X))", CASIM_RAD, out) == CASIM_OK, "SQRT(SQ(X)) did not simplify");
    {
        abn v, want = abn_from_int(2);
        ce_set('X', "-2");
        CHECK(ce_eval(out, &v) == ABN_OK && abn_cmp(&v, &want) == 0, "SQRT(SQ(X)) at X=-2 is not 2: %s", out);
    }
    is("X÷X", "1");                     /* 1 where X = 0 too: an extension */
    is("EXP(LN(X))", "X");              /* X where X <= 0 too */
    is("0×LN(X)", "0");
}

static void visible_pairs(void)
{
    FILE *fp = fopen("work/002-examples.txt", "r");
    char line[4096];
    int n = 0;
    CHECK(fp != NULL, "work/002-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[6], *s = line;
        char out[OUT];
        abn got, want;
        int k = 0, angle;
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 6) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 6) continue;
        angle = strcmp(f[3], "DEG") == 0 ? CASIM_DEG : strcmp(f[3], "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        n++;
        pair(f[1], f[2], angle);
        /* the line's own f, at its own point */
        ce_angle(angle);
        for (char *a = strtok(f[4], ";"); a; a = strtok(NULL, ";"))
            if (a[0] >= 'A' && a[0] <= 'Z' && a[1] == '=') ce_set(a[0], a + 2);
        want = ce_num(f[5]);
        CHECK(simp(f[1], angle, out) == CASIM_OK && ce_eval(out, &got) == ABN_OK && ce_close(&got, &want, -24),
              "%s: %s at its point is %s, want %s", f[0], out, ce_text(&got), f[5]);
    }
    fclose(fp);
    CHECK(n == 12, "%d visible pairs read, want 12", n);
}

int main(void)
{
    ce_init();
    rules();
    folding();
    nesting();
    domain();
    visible_pairs();
    return check_done();
}

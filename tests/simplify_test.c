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
    CHECK(!bare_chain(out), "simplify %s = %s: a bare chain of powers", in, out);
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
    CHECK(!bare_chain(oa), "simplify %s = %s: a bare chain of powers", a, oa);
    CHECK(same_value(a, oa, angle, -24), "simplify %s = %s changes the value", a, oa);
    CHECK(simp(oa, angle, again) == CASIM_OK && strcmp(again, oa) == 0, "not idempotent: %s -> %s", oa, again);
}

/* Decision 52 (abacus #4293): a chain of powers prints with explicit parentheses, (a^b)^c or a^(b^c), never
   a bare a^b^c, so the text reads the same in a mode that groups powers right to left. A bare chain still
   parses left to right (the core's reading, until unit 031). */
static void power_chains(void)
{
    is("X^Y^Z", "(X^Y)^Z");
    is("(X^Y)^Z", "(X^Y)^Z");
    is("X^(Y^Z)", "X^(Y^Z)");
    is("X^Y^Z^W", "((X^Y)^Z)^W");
    is("X^(Y^(Z^W))", "X^(Y^(Z^W))");
    is("(X^2)^Y", "(X^2)^Y");
    is("(X^Y)^0.3", "(X^Y)^0.3");
    is("2^X^Y", "(2^X)^Y");
    is("X^Y^Z×W", "W×(X^Y)^Z");
    is("SQRT(X)^Y", "SQRT(X)^Y");                 /* no ^ at its top: no parentheses */
    is("(X^Y^Z)^(W^V)", "((X^Y)^Z)^(W^V)");
    /* the detector, both ways */
    CHECK(bare_chain("X^Y^Z") && bare_chain("SIN(X)^2^Y") && bare_chain("X×Y^Z^W") && bare_chain("(X+Y^2^Z)"),
          "bare_chain misses a chain");
    CHECK(!bare_chain("(X^Y)^Z") && !bare_chain("X^(Y^Z)") && !bare_chain("X^2×Y^2") && !bare_chain("X^2÷Y^2") &&
              !bare_chain("X^2+Y^2") && !bare_chain("SIN(X^2)^Y"),
          "bare_chain fires on no chain");
    {
        /* seeded nests of powers through simplify, expand and derive: no bare chain, the value kept */
        static const char *const AT[] = {"X", "Y", "2", "0.5", "(X+1)", "SIN(X)", "SQRT(Y)", "(-X)", "Z"};
        unsigned long long r = 52;
        int bad = 0;
        for (int n = 0; n < 1500 && bad < 3; n++) {
            char t[256] = "", out[OUT];
            int depth = 2 + (int)((r >> 33) % 4), len = 0;
            for (int i = 0; i < depth; i++) {
                r = r * 6364136223846793005ULL + 1442695040888963407ULL;
                switch ((r >> 40) % 3) {
                case 0: len += snprintf(t + len, sizeof t - (size_t)len, "%s%s", i ? "^" : "", AT[(r >> 33) % 9]); break;
                case 1: len += snprintf(t + len, sizeof t - (size_t)len, "%s(%s^%s)", i ? "^" : "", AT[(r >> 33) % 9],
                                        AT[(r >> 45) % 9]); break;
                default: len += snprintf(t + len, sizeof t - (size_t)len, "%s%s×%s", i ? "^" : "", AT[(r >> 33) % 9],
                                         AT[(r >> 45) % 9]); break;
                }
            }
            r = r * 6364136223846793005ULL + 1442695040888963407ULL;
            if (simp(t, CASIM_RAD, out) == CASIM_OK && (bare_chain(out) || !same_value(t, out, CASIM_RAD, -20)) && ++bad)
                CHECK(false, "simplify %s = %s: a bare chain or another value", t, out);
            if (casim_expand(t, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
                (bare_chain(out) || !same_value(t, out, CASIM_RAD, -20)) && ++bad)
                CHECK(false, "expand %s = %s: a bare chain or another value", t, out);
            if (casim_derive(t, 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK && bare_chain(out) && ++bad)
                CHECK(false, "derive %s = %s: a bare chain", t, out);
        }
        CHECK(bad == 0, "%d power nests printed a bare chain or another value", bad);
    }
}

/* in simplifies, and its result is a fixed point (the text itself is not the point). */
static void fixed(const char *in)
{
    char out[OUT], again[OUT];
    casim_status st = simp(in, CASIM_RAD, out);
    CHECK(st == CASIM_OK, "simplify %s: status %d", in, (int)st);
    if (st != CASIM_OK) return;
    CHECK(same_value(in, out, CASIM_RAD, -24), "simplify %s = %s changes the value", in, out);
    CHECK(simp(out, CASIM_RAD, again) == CASIM_OK && strcmp(again, out) == 0, "not idempotent: %s -> %s -> %s", in,
          out, again);
}

/* The fresh-eyes review's findings (2026-10-06), each now a test. */
static void review(void)
{
    char t[4096], r[4096];
    /* 1: a number kept apart (too long to fold) keeps its sign in a product */
    is("X×(-3E18)×(-3E18)", "X×3000000000000000000×3000000000000000000");     /* one sign rule (review 2) */
    is("(-3E18)×(-3E18)", "3000000000000000000×3000000000000000000");
    is("X×(-3E18)×3E18", "X×(-3000000000000000000)×3000000000000000000");
    is("117649×117649÷13319113015296÷13319113015296", "(1÷13319113015296)×(1÷13319113015296)×117649×117649");
    /* 2: any merge gets another pass (a merge can make a power of another base, or a sum not primitive) */
    is("SQRT(X^2)×SQRT(X^2)×X", "X^3");
    is("SQRT(X^Y)×SQRT(X^Y)×X", "X^(Y+1)");
    is("Y×(Y^0.5)^1.5÷(Y^0.5)^0.5", "Y^1.5");
    is("SQRT(2×X+2)×SQRT(2×X+2)×Y", "2×Y×(X+1)");
    is("SQRT(2×X+2)×SQRT(2×X+2)×(X+1)", "2×(X+1)^2");
    /* 3: no limit on the numbers folded together */
    strcpy(t, "1");
    strcpy(r, "X");
    for (int i = 2; i <= 25; i++) sprintf(t + strlen(t), "+%d", i);
    strcat(t, "+X");
    is(t, "X+325");
    for (int i = 25; i >= 1; i--) sprintf(r + strlen(r), "+%d", i);
    is(r, "X+325");
    strcpy(t, "X");
    for (int i = 0; i < 25; i++) strcat(t, "×2");
    is(t, "33554432×X");
    /* 4: a fold that would overflow folds nothing: every number apart, in order, a fixed point */
    is("(-4E18)+(-4E18)+3E18+X", "X-4000000000000000000-4000000000000000000+3000000000000000000");
    /* a run of like terms whose sum fits though a partial sum would not: summed in order of value */
    pair("3E18×X+3E18×X-3E18×X", "-3E18×X+3E18×X+3E18×X", CASIM_RAD);
    is("3E18×X+3E18×X-3E18×X", "3000000000000000000×X");
    is("(-3E18)+(-3E18)+3E18+3E18+X", "X");          /* the numbers too: no partial sum past the total */
    /* the delta review: the order of value is what decides (deleting the sort must fail) */
    is("3E18×X+3E18×X-3E18×X-3E18×X+3E18×X", "3000000000000000000×X");
    pair("4E18×X+(-4E18)×X+4E18×X+4E18×X", "4E18×X+4E18×X+4E18×X-4E18×X", CASIM_RAD);
    is("4E18×X+(-4E18)×X+4E18×X+4E18×X",
       "-4000000000000000000×X+4000000000000000000×X+4000000000000000000×X+4000000000000000000×X");
    /* ... and a denominator's terms are added together first: 2E18÷7-2E18÷7 cancels before 3E18 meets it */
    is("2E18÷7×X-2E18÷7×X+3E18×X", "3000000000000000000×X");
    is("2E18÷7-2E18÷7+3E18+X", "X+3000000000000000000");
    is("1÷3037000499-1÷3037000499+1÷3037000493+X", "X+1÷3037000493");
    pair("3E18×X+2E18÷7×X-2E18÷7×X", "-2E18÷7×X+3E18×X+2E18÷7×X", CASIM_RAD);
    /* a sum made primitive is a sum again: a run left apart that fits after the division merges, so
       simplifying the result changes nothing */
    fixed("(-4E18×Y-3E18×Y)×X");
    fixed("(5×Y÷6+3000000000000000000×Y)×X");
    fixed("(-4E18×Y-3E18×Y)^2");
    fixed("(4E18×Y+4E18×Y+1)×X");
    /* the third pass: groups' totals that overflow fall back to the walk by value over every term */
    is("2E18+1E18-4500000000000000001÷2+X", "X+1499999999999999999÷2");
    is("(-1E18)×X+(5÷6)×X+(0.5)×X+(1÷3)×X+(4E18÷3)×X", "333333333333333335×X");
    is("3E18×X+3E18×X-3E18×X-2E18×X+2E18÷7×X-2E18÷7×X", "1000000000000000000×X");   /* only by denominator, from both ends */
    /* a merged run whose numbers stay apart sorts again, and merges with its new neighbour */
    fixed("X×2E18×4E18+X×3E18×4E18+X×3E18×4E18");
    fixed("X×2×3E18×4E18+X×3E18×4E18+X×3E18×4E18");
    /* a sum with numbers apart keeps its sign outside, so its printing reads back */
    fixed("(X+4E18×4E18×Y)÷(-Z)");
    pair("-Z×(X+4E18×4E18×Y)", "Z×(-X-4E18×4E18×Y)", CASIM_RAD);
    {
        /* ... and the sign is a plain term's, which negating does not move: a term whose numbers are
           apart sorts elsewhere once negated, and a sign taken from it flipped at every pass, filling
           the arena (fuzz: 10 of 20,000 collects). Within the 8,800 B budget: */
        static unsigned char small_arena[8800];
        char out[OUT];
        const char *t = "(-4E18×Y+3×Y÷3037000499+Y×(-3)×4E18+Y×3×3E18+Y×3×4E18)×X";
        CHECK(casim_simplify(t, CASIM_RAD, out, OUT, small_arena, sizeof small_arena) == CASIM_OK, "simplify %s in 8,800 B: %s", t,
              out);
        CHECK(casim_collect(t, 'X' - 'A', CASIM_RAD, out, OUT, small_arena, sizeof small_arena) == CASIM_OK,
              "collect %s in 8,800 B: %s", t, out);
        fixed(t);
    }
    is("3E18+X+(-4E18)+(-4E18)", "X-4000000000000000000-4000000000000000000+3000000000000000000");
    /* 5: 0 to a negative power has no value. It stayed as written until CAS 008, whose rule 5 (ruled uniform,
       abacus #5462) refuses it with UNDEFINED, typed or made; a symbolic exponent stays as written. The second review
       (2026-10-06) added -1 and -1/2; a regrouped product still merges with its power; a-b and -b+a agree with
       numbers that do not fold; a like-term merge with them stays canonical */
    {
        static const char *const U[] = {"0^(-2)", "(X-X)^(-2)", "0^(-1)", "X÷0", "0^(-1÷2)"};
        for (unsigned i = 0; i < sizeof U / sizeof U[0]; i++) {
            char out[OUT];
            casim_status st = simp(U[i], CASIM_RAD, out);
            CHECK(st == CASIM_UNDEFINED && out[0] == '\0', "simplify %s: status %d \"%s\", want UNDEFINED", U[i], (int)st, out);
        }
        is("0^(-X)", "0^(-X)");
    }
    pair("(SQRT(2×Z)×SQRT(2×Z))÷SQRT(2×Z)", "SQRT(2×Z)×SQRT(2×Z)÷SQRT(2×Z)", CASIM_RAD);
    pair("(SQRT(Y×Z)×SQRT(Y×Z))÷SQRT(Y×Z)", "SQRT(Y×Z)×(SQRT(Y×Z)÷SQRT(Y×Z))", CASIM_RAD);
    pair("X-4E18×4E18", "-4E18×4E18+X", CASIM_RAD);
    pair("Y-X×4E18×4E18", "-(X×4E18×4E18)+Y", CASIM_RAD);
    is("4E18×X×4E18×Y+4E18×Y×4E18×X", "X×Y×2×4000000000000000000×4000000000000000000");
    is("2×SQRT(1.5)÷3", "2×SQRT(1.5)÷3");          /* a power of a number among numbers (pair test) */
    /* the third review: a 19-digit decimal the printer writes reads back as the same number (CAS 019: made from a
       typed decimal, 2^-18; from 531441÷262144 alone it is that fraction) */
    is("531441×0.000003814697265625×X", "2.027286529541015625×X");
    is("531441×0.000003814697265625+531441÷262144", "4.05457305908203125");
    is("531441÷262144×X", "531441×X÷262144");
    is("X-0.5×π-SQRT(π+5)-0.5×0.25^1.5×2", "X-π÷2-SQRT(π+5)-0.125");       /* CAS 008 rule 1: a perfect power folds */
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
    is("X÷2", "X÷2");
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
    is("2^-2", "1÷4");
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
    review();
    rules();
    folding();
    nesting();
    domain();
    visible_pairs();
    power_chains();
    return check_done();
}

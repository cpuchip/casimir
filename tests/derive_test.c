/* Unit CAS 001, casim's own tests: every rule alone and under the chain rule, the quotient and power
 * rules, the DEG and GRAD factors, L=R, each refusal, SYNTAX where the core says, TOO_LONG, and
 * parse-print round trips. A derivative is judged by its value through the core's eqn_eval against
 * the rule's text (the standard differentiation rules, e.g. Wikipedia "Differentiation rules" and
 * "Differentiation of trigonometric functions"), at three points. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 1024
static unsigned char arena[16384];

/* Text with U replaced by u and D by d (each in parentheses). */
static const char *subst(const char *tmpl, const char *u, const char *d)
{
    static char buf[2][OUT];
    static int k;
    char *o = buf[k = !k];
    size_t n = 0;
    for (const char *s = tmpl; *s; s++) {
        const char *r = *s == 'U' ? u : *s == 'D' ? d : NULL;
        if (r) n += (size_t)snprintf(o + n, OUT - n, "(%s)", r);
        else o[n++] = *s;
    }
    o[n] = '\0';
    return o;
}

static const char *const XS[3] = {"0.3", "0.55", "0.8"};

/* d/d(var) of expr in angle equals want's value at three points. */
static void same(const char *expr, char var, int angle, const char *want)
{
    char out[OUT];
    casim_status st = casim_derive(expr, var - 'A', angle, out, sizeof out, arena, sizeof arena);
    CHECK(st == CASIM_OK, "d/d%c %s: status %d (%s)", var, expr, (int)st, out);
    if (st != CASIM_OK) return;
    ce_angle(angle);
    for (int i = 0; i < 3; i++) {
        abn g, w;
        abn_status sg, sw;
        ce_set('X', XS[i]);
        ce_set('Y', "1.7");
        ce_set('A', "2.5");
        ce_set('B', "0.4");
        ce_set('i', "0.9");
        sg = ce_eval(out, &g);
        sw = ce_eval(want, &w);
        CHECK(sw == ABN_OK, "want %s does not evaluate at X=%s (%d)", want, XS[i], (int)sw);
        CHECK(sg == ABN_OK && sw == ABN_OK && ce_close(&g, &w, -26),
              "d/d%c %s at X=%s: got %s = %s, want %s = %s", var, expr, XS[i], out,
              sg == ABN_OK ? ce_text(&g) : "error", want, sw == ABN_OK ? ce_text(&w) : "error");
    }
}

/* Each function's rule: f(U) and f'(U) with D for U's derivative. */
static const char *const RULES[][2] = {
    {"SQ(U)", "2×U×D"},
    {"SQRT(U)", "D÷(2×SQRT(U))"},
    {"INV(U)", "-D÷SQ(U)"},
    {"LN(U)", "D÷U"},
    {"LOG(U)", "D÷(U×LN(10))"},
    {"EXP(U)", "EXP(U)×D"},
    {"ALOG(U)", "ALOG(U)×LN(10)×D"},
    {"SIN(U)", "COS(U)×D"},
    {"COS(U)", "-SIN(U)×D"},
    {"TAN(U)", "D÷SQ(COS(U))"},
    {"ASIN(U)", "D÷SQRT(1-SQ(U))"},
    {"ACOS(U)", "-D÷SQRT(1-SQ(U))"},
    {"ATAN(U)", "D÷(1+SQ(U))"},
    {"SINH(U)", "COSH(U)×D"},
    {"COSH(U)", "SINH(U)×D"},
    {"TANH(U)", "D÷SQ(COSH(U))"},
    {"ASINH(U)", "D÷SQRT(SQ(U)+1)"},
    {"ACOSH(1+U)", "D÷SQRT(SQ(1+U)-1)"},
    {"ATANH(U)", "D÷(1-SQ(U))"},
};

static void rules(void)
{
    for (size_t i = 0; i < sizeof RULES / sizeof RULES[0]; i++) {
        same(subst(RULES[i][0], "X", "1"), 'X', CASIM_RAD, subst(RULES[i][1], "X", "1"));
        same(subst(RULES[i][0], "X×X", "2×X"), 'X', CASIM_RAD, subst(RULES[i][1], "X×X", "2×X"));
        /* the other variable's function is a constant */
        same(subst(RULES[i][0], "B", "0"), 'X', CASIM_RAD, "0");
    }
    /* ABS: SGN(u)×u' (its two signs at X = 0.3 and 0.8) */
    same("ABS(X-0.5)", 'X', CASIM_RAD, "SGN(X-0.5)");
    same("ABS(0.5-X×X)", 'X', CASIM_RAD, "SGN(0.5-X×X)×(-2×X)");
}

static void arithmetic(void)
{
    same("X", 'X', CASIM_RAD, "1");
    same("Y", 'X', CASIM_RAD, "0");
    same("5", 'X', CASIM_RAD, "0");
    same("π", 'X', CASIM_RAD, "0");
    same("1.5E-3", 'X', CASIM_RAD, "0");
    same("X+Y", 'X', CASIM_RAD, "1");
    same("X-Y", 'X', CASIM_RAD, "1");
    same("Y-X", 'X', CASIM_RAD, "-1");
    same("-X", 'X', CASIM_RAD, "-1");
    same("--X", 'X', CASIM_RAD, "1");
    same("X×X", 'X', CASIM_RAD, "2×X");
    same("Y×X", 'X', CASIM_RAD, "Y");          /* 0×X + Y×1: 0×u must fold to 0, never u */
    same("X×Y", 'X', CASIM_RAD, "Y");
    same("3×X×X", 'X', CASIM_RAD, "6×X");
    same("A×B", 'B', CASIM_RAD, "A");
    same("i×X+SIN(i)", 'X', CASIM_RAD, "i");      /* the 33s's variable i is a constant here */
    same("X+Y=Y×X", 'Y', CASIM_RAD, "1=X");
    /* the quotient rule */
    same("X÷Y", 'X', CASIM_RAD, "1÷Y");
    same("Y÷X", 'X', CASIM_RAD, "-Y÷SQ(X)");
    same("X÷(X+1)", 'X', CASIM_RAD, "1÷SQ(X+1)");
    same("(X+1)÷(X-1)", 'X', CASIM_RAD, "-2÷SQ(X-1)");
    same("SIN(X)÷X", 'X', CASIM_RAD, "(X×COS(X)-SIN(X))÷SQ(X)");
    /* the power rule, and powers with the variable in the exponent */
    same("X^3", 'X', CASIM_RAD, "3×SQ(X)");
    same("X^0.5", 'X', CASIM_RAD, "0.5÷SQRT(X)");
    same("X^-2", 'X', CASIM_RAD, "-2÷(X×X×X)");
    same("(X×X+1)^3", 'X', CASIM_RAD, "6×X×SQ(X×X+1)");
    same("2^X", 'X', CASIM_RAD, "2^X×LN(2)");
    same("Y^X", 'X', CASIM_RAD, "Y^X×LN(Y)");
    same("X^X", 'X', CASIM_RAD, "X^X×(LN(X)+1)");
    same("(X+1)^Y", 'X', CASIM_RAD, "Y×(X+1)^(Y-1)");
    same("X^Y^2", 'X', CASIM_RAD, "2×Y×X^(2×Y-1)");   /* (X^Y)^2, left to right */
    same("-X^2", 'X', CASIM_RAD, "-2×X");             /* -(X^2) */
    same("2^-X", 'X', CASIM_RAD, "-(2^-X)×LN(2)");
    /* where the function is defined, so is its derivative (ours, beyond rule 5): u-u is 0, 0÷u is 0,
       and d/dx 0^v is 0; ASIN(1) and ASINH(-ASIN(1)) sit at ASIN's edge, where SQRT(1-SQ(1)) is 0 */
    same("ASIN(X÷X)", 'X', CASIM_RAD, "0");
    same("ASINH(-ASIN(Y÷Y))+X", 'Y', CASIM_DEG, "0");
    same("ACOS(SIN(0^X))", 'X', CASIM_GRAD, "0");
    same("SQRT(0÷X)", 'X', CASIM_RAD, "0");
}

static void angles(void)
{
    const char *k[2] = {"π÷180", "π÷200"}, *kk[2] = {"180÷π", "200÷π"};
    int modes[2] = {CASIM_DEG, CASIM_GRAD};
    for (int m = 0; m < 2; m++) {
        char w[OUT];
        const char *u = "X×X", *d = "2×X";
        snprintf(w, sizeof w, "COS(U)×D×%s", k[m]);
        same(subst("SIN(U)", u, d), 'X', modes[m], subst(w, u, d));
        snprintf(w, sizeof w, "-SIN(U)×D×%s", k[m]);
        same(subst("COS(U)", u, d), 'X', modes[m], subst(w, u, d));
        snprintf(w, sizeof w, "D×%s÷SQ(COS(U))", k[m]);
        same(subst("TAN(U)", u, d), 'X', modes[m], subst(w, u, d));
        snprintf(w, sizeof w, "D÷SQRT(1-SQ(U))×%s", kk[m]);
        same(subst("ASIN(U)", u, d), 'X', modes[m], subst(w, u, d));
        snprintf(w, sizeof w, "-D÷SQRT(1-SQ(U))×%s", kk[m]);
        same(subst("ACOS(U)", u, d), 'X', modes[m], subst(w, u, d));
        snprintf(w, sizeof w, "D÷(1+SQ(U))×%s", kk[m]);
        same(subst("ATAN(U)", u, d), 'X', modes[m], subst(w, u, d));
        /* the hyperbolic functions take no angle */
        same("SINH(X)", 'X', modes[m], "COSH(X)");
        snprintf(w, sizeof w, "COS(SIN(X))×COS(X)×%s×%s", k[m], k[m]);
        same("SIN(SIN(X))", 'X', modes[m], w);
    }
}

/* L=R gives L'=R', and the = is kept. */
static void equations(void)
{
    char out[OUT];
    same("X^2=Y×X", 'X', CASIM_RAD, "2×X=Y");
    CHECK(casim_derive("X^2=Y×X", 'X' - 'A', CASIM_RAD, out, sizeof out, arena, sizeof arena) == CASIM_OK &&
          strchr(out, '=') && strchr(out, '=') == strrchr(out, '='), "L=R kept one =: %s", out);
    CHECK(casim_derive("SIN(X)", 'X' - 'A', CASIM_RAD, out, sizeof out, arena, sizeof arena) == CASIM_OK &&
          !strchr(out, '='), "an expression gained an =: %s", out);
}

static void refused(const char *expr, const char *name)
{
    char out[OUT];
    casim_status st = casim_derive(expr, 'X' - 'A', CASIM_RAD, out, sizeof out, arena, sizeof arena);
    CHECK(st == CASIM_NOT_DIFFERENTIABLE && strcmp(out, name) == 0, "%s: status %d out \"%s\", want refused \"%s\"",
          expr, (int)st, out, name);
}

static void refusals(void)
{
    static const char *const k1[] = {"IP", "FP", "RND", "INTG", "SGN", "SEED", "NOT", "ARG", "→DEG", "→RAD",
                                     "HMS→", "→HMS", "→L", "→GAL", "→MILE", "→KM", "→KG", "→LB", "→°C",
                                     "→°F", "→CM", "→IN", "x̂", "ŷ"};
    static const char *const k2[] = {"IDIV", "RMDR", "%CHG", "XROOT", "nCr", "nPr", "AND", "XOR", "OR",
                                     "NAND", "NOR", "%"};
    static const char *const k0[] = {"RAND", "sx", "sy", "σx", "σy", "x̄w", "x̄", "ȳ", "r", "m", "b", "n",
                                     "Σx²", "Σy²", "Σxy", "Σx", "Σy", "REGX", "REGY", "REGZ", "REGT"};
    char e[64];
    for (size_t i = 0; i < sizeof k1 / sizeof k1[0]; i++) {
        snprintf(e, sizeof e, "1+%s(X)", k1[i]);
        refused(e, k1[i]);
    }
    for (size_t i = 0; i < sizeof k2 / sizeof k2[0]; i++) {
        snprintf(e, sizeof e, "%s(X,2)×X", k2[i]);
        refused(e, k2[i]);
    }
    for (size_t i = 0; i < sizeof k0 / sizeof k0[0]; i++) {
        snprintf(e, sizeof e, "X+%s", k0[i]);
        refused(e, k0[i]);
    }
    refused("X×1Fh", "1Fh");
    refused("1100b+X", "1100b");
    refused("X+1i2", "1i2");
    refused("X+3θ10", "3θ10");
    refused("X!", "!");
    refused("[X,1]", "[");
    refused("X+⒤", "⒤");
    /* a refused name of the other variable is refused all the same (the unit's rule 4) */
    refused("IP(Y)+X", "IP");
}

/* SYNTAX at the core's own offset (eqn_eval with check_only), checked before any refusal. */
static void syntax(void)
{
    static const char *const bad[] = {"2X", "", "SIN(X", "X+", "X=Y=Z", "SIN X", "(X", "X)", "×X", "X^",
                                      "IP(X", "X+,", "1..2.3.4", "X÷÷Y", "SQ()", "X Y", "2i"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char out[OUT], want[16];
        ab_val v;
        int errpos = -1;
        abn_status cs = eqn_eval(ce_calc, bad[i], false, true, &v, &errpos);
        casim_status st = casim_derive(bad[i], 'X' - 'A', CASIM_RAD, out, sizeof out, arena, sizeof arena);
        CHECK(cs == ABN_SYNTAX, "the core accepts \"%s\" (%d): not a syntax case", bad[i], (int)cs);
        snprintf(want, sizeof want, "%d", errpos);
        CHECK(st == CASIM_SYNTAX && strcmp(out, want) == 0, "\"%s\": status %d out \"%s\", want SYNTAX %s", bad[i],
              (int)st, out, want);
    }
}

static void limits(void)
{
    char out[OUT], small[4];
    unsigned char tiny[16];
    CHECK(casim_derive("SIN(X×X)", 'X' - 'A', CASIM_RAD, small, sizeof small, arena, sizeof arena) == CASIM_TOO_LONG &&
          small[0] == '\0', "a 4-byte out: not TOO_LONG");
    CHECK(casim_derive("SIN(X×X)", 'X' - 'A', CASIM_RAD, out, sizeof out, tiny, sizeof tiny) == CASIM_TOO_LONG &&
          out[0] == '\0', "a 16-byte arena: not TOO_LONG");
    CHECK(casim_derive("X", 26, CASIM_RAD, out, sizeof out, arena, sizeof arena) == CASIM_BAD_ARG, "var 26 accepted");
    CHECK(casim_derive("X", -1, CASIM_RAD, out, sizeof out, arena, sizeof arena) == CASIM_BAD_ARG, "var -1 accepted");
    CHECK(casim_derive("X", 23, 3, out, sizeof out, arena, sizeof arena) == CASIM_BAD_ARG, "angle 3 accepted");
    /* an out exactly the size of the text and its NUL is enough; one byte less is not */
    if (casim_derive("SIN(X)", 'X' - 'A', CASIM_RAD, out, sizeof out, arena, sizeof arena) == CASIM_OK) {
        size_t n = strlen(out) + 1;
        char exact[OUT];
        CHECK(casim_derive("SIN(X)", 'X' - 'A', CASIM_RAD, exact, n, arena, sizeof arena) == CASIM_OK &&
              strcmp(exact, out) == 0, "out of exactly %zu bytes refused", n);
        CHECK(casim_derive("SIN(X)", 'X' - 'A', CASIM_RAD, exact, n - 1, arena, sizeof arena) == CASIM_TOO_LONG,
              "out of %zu bytes accepted", n - 1);
    } else CHECK(false, "SIN(X) did not derive");
}

/* An arena of every size from 0 up gives TOO_LONG until it is enough, then the one answer, for derive
   and simplify: never a crash, never another text. */
static void starved(void)
{
    static const char *const eqs[] = {
        "((π)-(ABS(ACOSH(Y))))+(Y)",
        "(SQ((ASIN(X))-(SIN(Y))))÷((0.25)+(((X)-(X))-(SQRT(X))))",
        "X^X^X÷(X+1)-COS(-X)=SQRT(X)×LN(X÷X)",
        "--X×-(Y-X)^-2",
    };
    for (size_t i = 0; i < sizeof eqs / sizeof eqs[0]; i++) {
        char want[OUT], got[OUT], wantr[OUT], gotr[OUT];
        int bad = 0, badr = 0;
        casim_status st = casim_derive(eqs[i], 'X' - 'A', CASIM_DEG, want, sizeof want, arena, sizeof arena);
        casim_status sr = casim_simplify(eqs[i], CASIM_DEG, wantr, sizeof wantr, arena, sizeof arena);
        CHECK(st == CASIM_OK && sr == CASIM_OK, "%s: status %d / %d with the full arena", eqs[i], (int)st, (int)sr);
        for (size_t n = 0; n <= 8192; n++) {
            casim_status a = casim_derive(eqs[i], 'X' - 'A', CASIM_DEG, got, sizeof got, arena, n);
            casim_status r = casim_simplify(eqs[i], CASIM_DEG, gotr, sizeof gotr, arena, n);
            if (!((a == CASIM_TOO_LONG && got[0] == '\0') || (a == CASIM_OK && strcmp(got, want) == 0))) bad++;
            if (!((r == CASIM_TOO_LONG && gotr[0] == '\0') || (r == CASIM_OK && strcmp(gotr, wantr) == 0))) badr++;
        }
        CHECK(bad == 0 && badr == 0, "%s: %d derive and %d simplify arena sizes gave another answer", eqs[i], bad, badr);
    }
}

/* Simplify keeps the value (to 1E-26: it may reorder the arithmetic) and is a fixed point, on texts that
   lean on the core's precedence (unary minus, ^ left to right, ÷ and - on the right). */
static void round_trips(void)
{
    static const char *const texts[] = {
        "-X^2", "2^-X", "2^-X^2", "X-Y-Z", "X-(Y-Z)", "X÷Y÷Z", "X÷(Y×Z)", "X^Y^Z", "X^(Y^Z)", "--X", "X--Y",
        "2×-X", "-(X+Y)", "(-X)^2", "-X^-Y", "SQ(-X)", "X=Y+1", "1.5E-3×X", ".5×X", "1.2.3+X", "X+-Y×Z",
        "((X))", "SIN(X)^2", "-SIN(X)^2", "X×(Y+Z)×(Y-Z)", "(X+Y)÷(Y-Z)^2", "π×X", "X^0.5^2", "(X^Y)^Z",
        "-(-X)", "X-(-Y)", "X×(-Y)", "X^(-Y)", "(Y-X)^-(Y+Z)", "LOG(X)+ALOG(-X)-EXP(X)÷LN(Y)", "ABS(X-Y)",
        "1E5×X", "2.5E-30+X", "X-Y+Z", "X-(Y+Z)", "X÷Y×Z", "X÷(Y÷Z)", "X×Y÷Z", "X×(Y÷Z)", "-X×Y", "-(X×Y)",
    };
    ce_angle(CASIM_RAD);
    ce_set('X', "0.7");
    ce_set('Y', "1.3");
    ce_set('Z', "2.1");
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        char out[OUT], again[OUT];
        abn a, b;
        abn_status sa, sb;
        casim_status st = casim_simplify(texts[i], CASIM_RAD, out, sizeof out, arena, sizeof arena);
        CHECK(st == CASIM_OK, "simplify %s: status %d", texts[i], (int)st);
        if (st != CASIM_OK) continue;
        sa = ce_eval(texts[i], &a);
        sb = ce_eval(out, &b);
        CHECK(sa == ABN_OK && sb == ABN_OK && ce_close(&b, &a, -26), "simplify %s -> %s: %s vs %s", texts[i], out,
              sa == ABN_OK ? ce_text(&a) : "error", sb == ABN_OK ? ce_text(&b) : "error");
        CHECK(casim_simplify(out, CASIM_RAD, again, sizeof again, arena, sizeof arena) == CASIM_OK &&
              strcmp(out, again) == 0, "simplify is not a fixed point: %s -> %s -> %s", texts[i], out, again);
    }
}

int main(void)
{
    ce_init();
    rules();
    arithmetic();
    angles();
    equations();
    refusals();
    syntax();
    limits();
    starved();
    round_trips();
    return check_done();
}
